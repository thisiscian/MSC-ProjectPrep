/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 1999-2009
 * Frank Loemker, Applied Computer Science, Faculty of Technology,
 * Bielefeld University, Germany
 *
 * This file is part of iceWing, a graphical plugin shell.
 *
 * iceWing is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * iceWing is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include "config.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "gui/Gtools.h"
#include "drv_v4l2_old.h"
#include "drv_v4l2.h"
#include "drv_firewire.h"
#include "drv_unicap.h"
#include "drv_mvimpact.h"
#include "drv_ueye.h"
#include "drv_aravis.h"
#include "drv_icube.h"
#include "AVOptions.h"
#include "AVCapture.h"

AVcapDriver AVdriver[] = {
	{'C', "PAL_COMPOSITE", PAL_COMPOSITE, NULL, NULL, NULL, NULL},
	{'S', "PAL_S_VIDEO", PAL_S_VIDEO, AVv4l2_old_open, AVv4l2_old_close, AVv4l2_old_capture, NULL},
	{'V', "V4L2", AV_V4L2, AVv4l2_open, AVv4l2_close, AVv4l2_capture, AVv4l2_set_property},
#ifdef WITH_ARAVIS
	{'A', "ARAVIS", AV_ARAVIS, AVaravis_open, AVaravis_close, AVaravis_capture, AVaravis_set_property},
#endif
#if defined(WITH_FIRE) || defined(WITH_FIRE2)
	{'F', "FIREWIRE", AV_FIREWIRE, AVfire_open, AVfire_close, AVfire_capture, AVfire_set_property},
#endif
#ifdef WITH_ICUBE
	{'I', "ICUBE", AV_ICUBE, AVicube_open, AVicube_close, AVicube_capture, AVicube_set_property},
#endif
#ifdef WITH_UNICAP
	{'U', "UNICAP", AV_UNICAP, AVuni_open, AVuni_close, AVuni_capture, AVuni_set_property},
#endif
#ifdef WITH_MVIMPACT
	{'M', "MVIMPACT", AV_MVIMPACT, AVmvimp_open, AVmvimp_close, AVmvimp_capture, NULL},
#endif
#ifdef WITH_UEYE
	{'E', "UEYE", AV_UEYE, AVueye_open, AVueye_close, AVueye_capture, NULL},
#endif
	{'\0', NULL, PAL_COMPOSITE}
};

/*********************************************************************
  If necessary, byte swap and bayer decode the data from src (of
  length srccnt), and save them in dst->data[0].
*********************************************************************/
void AVcap_mono16_ip (AVcapIP *ip, iwImage *dst, guchar *src, int srccnt, BOOL swap)
{
	if (swap) {
		guint16 *d;
		int i;

		if (ip->bayer == IW_IMG_BAYER_NONE)
			d = (guint16*)(dst->data[0]);
		else
			d = (guint16*)ip->buffer;
		for (i=0; i<srccnt; i++)
			d[i] = GUINT16_SWAP_LE_BE(((guint16*)src)[i]);
		src = (guchar*)d;
	}
	if (ip->bayer == IW_IMG_BAYER_NONE) {
		dst->ctab = IW_GRAY;
		if (!swap)
			memcpy (dst->data[0], src, dst->width*dst->height * 2);
	} else {
		dst->ctab = IW_RGB;
		iw_img_bayer_decode (src, dst, ip->bayer, ip->pattern);
	}
}

/*********************************************************************
  Parse "stereo", "bayer", and "pattern" options from devOptions.
  driver: Name for error messages
  Returns FALSE if a parsing error occured.
*********************************************************************/
BOOL AVcap_parse_ip (char *driver, char *devOptions, AVcapIP *ip)
{
	char str[256];

	if (!devOptions) return TRUE;

	*str = '\0';
	if (AVcap_optstr_get (devOptions, "stereo", "%s", str) >= 0) {
		ip->stereo = AV_STEREO_DEINTER;
		if (!strcasecmp ("raw", str))
			ip->stereo = AV_STEREO_RAW;
		else if (*str && strcasecmp ("deinter", str)) {
			AVcap_warning ("%s: Expected raw|deinter for stereo option",
						   driver);
			return FALSE;
		}
	}

	*str = '\0';
	if (AVcap_optstr_get (devOptions, "bayer", "%s", str) >= 0) {
		if (!iw_img_bayer_parse (str, &ip->bayer)) {
			AVcap_warning ("%s: Unknown bayer mode '%s' given",
						   driver, str);
			return FALSE;
		}
	}

	*str = '\0';
	if (AVcap_optstr_get (devOptions, "pattern", "%s", str) > 0) {
		if (!iw_img_bayer_pattern_parse (str, &ip->pattern)) {
			AVcap_warning ("%s: Unknown bayer pattern '%s' given",
						   driver, str);
			return FALSE;
		}
	}

	return TRUE;
}

/*********************************************************************
  Allocate ip->buffer if needed and adapt dev->{f_}[width|height]
  according to ip.
  needBuf: If IW_16U and bayerDecoding allocate ip->buffer unconditionally
  driver: Name for error messages
  Returns FALSE on error.
*********************************************************************/
BOOL AVcap_init_ip (char *driver, VideoDevData *dev, AVcapIP *ip, BOOL needBuf)
{
	int buffer_size = 0;
	float dw = 1, dh = 1;

	ip->buffer = NULL;
	if (ip->stereo != AV_STEREO_NONE) {
		dh *= 2;
		if (ip->stereo == AV_STEREO_DEINTER)
			buffer_size = dev->width*dev->height*dh;
	}
	if (dev->type == IW_16U
		&& ip->bayer != IW_IMG_BAYER_NONE
		&& (needBuf || G_BYTE_ORDER != G_BIG_ENDIAN)) {
		buffer_size = dev->width*dev->height*dh*2;
	}
	if (ip->bayer == IW_IMG_BAYER_DOWN) {
		dw /= 2;
		dh /= 2;
	}
	if (buffer_size > 0) {
		ip->buffer = malloc (buffer_size);
		if (!ip->buffer) {
			AVcap_warning ("%s: Cannot allocate %d bytes buffer memory",
						   driver, buffer_size);
			return FALSE;
		}
	}
	dev->f_width *= dw;
	dev->f_height *= dh;
	dev->width *= dw;
	dev->height *= dh;

	return TRUE;
}

/*********************************************************************
  Add a property to cap->prop.
*********************************************************************/
static iwGrabProperty *AVCap_prop_add (AVcapData *cap, const char *name, gint64 priv, iwGrabPropertyType type)
{
	iwGrabProperty *prop;

	if (cap->prop_cnt+1 >= cap->prop_len) {
		cap->prop_len += 10;
		cap->prop = realloc (cap->prop, sizeof(iwGrabProperty)*cap->prop_len);
	}
	prop = cap->prop+cap->prop_cnt;
	cap->prop_cnt++;
	prop[1].name = NULL;
	if (name)
		prop->name = strdup (name);
	else
		prop->name = "unknown";
	prop->description = NULL;
	prop->flags = 0;
	prop->type = type;
	prop->priv = priv;
	return prop;
}

/*********************************************************************
  Add a property of the specified type to cap->prop.
*********************************************************************/
iwGrabProperty *AVCap_prop_add_category (AVcapData *cap, const char *name, gint64 priv)
{
	return AVCap_prop_add (cap, name, priv, IW_GRAB_CATEGORY);
}

iwGrabProperty *AVCap_prop_add_command (AVcapData *cap, const char *name, gint64 priv)
{
	return AVCap_prop_add (cap, name, priv, IW_GRAB_COMMAND);
}

iwGrabProperty *AVCap_prop_add_bool (AVcapData *cap, const char *name, BOOL val, BOOL def, gint64 priv)
{
	iwGrabProperty *prop = AVCap_prop_add (cap, name, priv, IW_GRAB_BOOL);
	prop->data.b.value = val;
	prop->data.b.def = def;
	return prop;
}

iwGrabProperty *AVCap_prop_add_int (AVcapData *cap, const char *name,
									gint64 val, gint64 min, gint64 max, gint64 def, gint64 priv)
{
	iwGrabProperty *prop = AVCap_prop_add (cap, name, priv, IW_GRAB_INT);
	prop->data.i.min = min;
	prop->data.i.max = max;
	prop->data.i.value = val;
	prop->data.i.def = def;
	return prop;
}

iwGrabProperty *AVCap_prop_add_double (AVcapData *cap, const char *name,
									   double val, double min, double max, double def, gint64 priv)
{
	iwGrabProperty *prop = AVCap_prop_add (cap, name, priv, IW_GRAB_DOUBLE);
	prop->data.d.min = min;
	prop->data.d.max = max;
	prop->data.d.value = val;
	prop->data.d.def = def;
	return prop;
}

iwGrabProperty *AVCap_prop_add_enum (AVcapData *cap, const char *name, int val, int def, gint64 priv)
{
	iwGrabProperty *prop = AVCap_prop_add (cap, name, priv, IW_GRAB_ENUM);
	prop->data.e.enums = calloc (1, sizeof(iwGrabPropertyEnum));
	prop->data.e.max = -1;
	prop->data.e.value = val;
	prop->data.e.def = def;
	return prop;
}
iwGrabProperty *AVCap_prop_append_enum (AVcapData *cap, const char *enumName, BOOL val, gint64 priv)
{
	iwGrabProperty *prop = cap->prop+cap->prop_cnt-1;
	iw_assert (prop->type == IW_GRAB_ENUM, "Last added property is not an enum");

	prop->data.e.max++;
	prop->data.e.enums = realloc (prop->data.e.enums, (prop->data.e.max+2)*sizeof(iwGrabPropertyEnum));
	if (enumName)
		prop->data.e.enums[prop->data.e.max].name = strdup (enumName);
	else
		prop->data.e.enums[prop->data.e.max].name = "unknown";
	prop->data.e.enums[prop->data.e.max].priv = priv;
	if (val)
		prop->data.e.value = prop->data.e.max;
	prop->data.e.enums[prop->data.e.max+1].name = NULL;
	return prop;
}

iwGrabProperty *AVCap_prop_add_string (AVcapData *cap, const char *name, const char *val, const char *def, gint64 priv)
{
	iwGrabProperty *prop = AVCap_prop_add (cap, name, priv, IW_GRAB_STRING);
	if (val)
		prop->data.s.value = strdup (val);
	else
		prop->data.s.value = NULL;
	if (def)
		prop->data.s.def = strdup (def);
	else
		prop->data.s.def = NULL;
	return prop;
}

/*********************************************************************
  Compare n1 and n2 case insensitive, ignoring all non digit/non alpha
  values from n2.
*********************************************************************/
static BOOL prop_cmp_name (const char *n1, const char *n2)
{
	for (; *n1; n1++) {
		while (*n2 && !isalnum((int)*n2) &&
			   (*n1|0x20) != (*n2|0x20) &&
			   *n2 != '.' && *n2 != ',' && *n2 != '-' && *n2 != '+' && *n2 != '_')
			n2++;
		if ((*n1|0x20) != (*n2|0x20))
			return FALSE;
		n2++;
	}
	return *n2 == '\0';
}

/*********************************************************************
  If name is a number of an existing property return that number,
  otherwise return the index of the property with the matching name,
  or -1 if no property matches.
*********************************************************************/
int AVCap_prop_lookup (AVcapData *cap, const char *name)
{
	char *end;
	int idx = strtol (name, &end, 10);
	if (*end != '\0') {
		for (idx=0; idx < cap->prop_cnt; idx++)
			if (!strcmp(cap->prop[idx].name, name))
				break;
		if (idx >= cap->prop_cnt)
			for (idx=0; idx < cap->prop_cnt; idx++)
				if (prop_cmp_name(name, cap->prop[idx].name))
					break;
	}
	if (idx >= 0 && idx < cap->prop_cnt)
		return idx;
	return -1;
}

/*********************************************************************
  Parse 'val' according the type from 'prop' and store the result in
  'dest'. 'dest may be identical to 'prop'. Return TRUE if val could
  be parsed and is valid according to 'prop'.
*********************************************************************/
BOOL AVCap_prop_parse (iwGrabProperty *prop, iwGrabProperty *dest, const char *val)
{
	BOOL ok = FALSE;
	char *end = "";

	if (dest->flags & IW_GRAB_READONLY) return false;

	switch (prop->type) {
		case IW_GRAB_COMMAND:
			ok = *val == '\0';
			break;
		case IW_GRAB_BOOL: {
			BOOL b;
			if (!*val) {
				b = prop->data.b.def;
				ok = TRUE;
			} else if (val[1] == '\0') {
				b = val[0] == '1';
				ok = val[0] == '0' || val[0] == '1';
			} else if (!strcasecmp(val, "true") || !strcasecmp(val, "on")) {
				b = TRUE;
				ok = TRUE;
			} else if (!strcasecmp(val, "false") || !strcasecmp(val, "off")) {
				b = FALSE;
				ok = TRUE;
			}
			if (ok) dest->data.b.value = b;
			break;
		}
		case IW_GRAB_INT: {
			gint64 i;
			if (*val)
				i = strtoll (val, &end, 10);
			else
				i = prop->data.i.def;
			ok = *end == '\0' && i >= prop->data.i.min && i <= prop->data.i.max;
			if (ok) dest->data.i.value = i;
			break;
		}
		case IW_GRAB_DOUBLE: {
			double d;
			if (*val)
				d = strtod (val, &end);
			else
				d = prop->data.d.def;
			ok = *end == '\0' && d >= prop->data.d.min && d <= prop->data.d.max;
			if (ok) dest->data.d.value = d;
			break;
		}
		case IW_GRAB_ENUM: {
			int e;
			if (*val) {
				double value;
				for (e = 0; prop->data.e.enums[e].name; e++) {
					if (!strcmp(val, prop->data.e.enums[e].name))
						goto parseDone;
				}
				for (e = 0; prop->data.e.enums[e].name; e++) {
					if (prop_cmp_name(val, prop->data.e.enums[e].name))
						goto parseDone;
				}
				value = strtod (val, &end);
				if (*end == '\0') {
					for (e = 0; prop->data.e.enums[e].name; e++) {
						double v = strtod (prop->data.e.enums[e].name, &end);
						if (iw_isequal(value, v) && *end == '\0')
							goto parseDone;
					}
				}
				e = strtol (val, &end, 10);
				if (*end == '\0') goto parseDone;

				e = -1;
			parseDone: ;
			} else
				e = prop->data.e.def;
			ok = e >= 0 && e <= prop->data.e.max;
			if (ok) dest->data.e.value = e;
			break;
		}
		case IW_GRAB_STRING:
			if (dest->data.s.value) free (dest->data.s.value);
			if (*val) {
				dest->data.s.value = strdup (val);
			} else if (prop->data.s.def) {
				dest->data.s.value = strdup (prop->data.s.def);
			} else
				dest->data.s.value = NULL;
			break;
		default:
			break;
	}
	return ok;
}

/*********************************************************************
  Print all properties from prop to stderr.
  driver: Prefix string for messages
*********************************************************************/
void AVCap_prop_print (char *driver, AVcapData *cap)
{
	int i, e, indent = 0;

	fprintf (stderr, "%s: Available properties:\n", driver);
	for (i=0; i < cap->prop_cnt; i++) {
		iwGrabProperty *prop = cap->prop+i;
		switch (prop->type) {
			case IW_GRAB_INT:
				if (prop->data.i.max < prop->data.i.min) continue;
				break;
			case IW_GRAB_DOUBLE:
				if (prop->data.d.max < prop->data.d.min) continue;
				break;
			case IW_GRAB_ENUM:
				if (!prop->data.e.enums || !prop->data.e.enums[0].name) continue;
				break;
			default: break;
		}
		if (prop->type == IW_GRAB_CATEGORY) {
			char *name = prop->name;
			while (*name) {
				if (*name == '<') {
					if (indent > 0) indent--;
				} else if (!isspace((int)*name)) {
					fprintf (stderr, "\t%*s%s:\n", indent*4, "", name);
					indent++;
					break;
				}
				name++;
			}
			continue;
		} else if (prop->flags & IW_GRAB_READONLY)
			fprintf (stderr, "\t%*sRO: %-12s", indent*4, "", prop->name);
		else
			fprintf (stderr, "\t%*s%2d: %-12s", indent*4, "", i, prop->name);
		switch (prop->type) {
			case IW_GRAB_COMMAND:
				fprintf (stderr, " command\n");
				break;
			case IW_GRAB_BOOL:
				fprintf (stderr, " [0 | 1] %s  Default %s\n",
						 prop->data.b.value ? "1":"0",
						 prop->data.b.def ? "1":"0");
				break;
			case IW_GRAB_INT: {
				gint64 val = prop->data.i.value;
				fprintf (stderr, " [%"G_GINT64_FORMAT" - %"G_GINT64_FORMAT"]",
						 (GINT64_PRINTTYPE)prop->data.i.min, (GINT64_PRINTTYPE)prop->data.i.max);
				if (val >= prop->data.i.min && val <= prop->data.i.max)
					fprintf (stderr, " %"G_GINT64_FORMAT"  Default %"G_GINT64_FORMAT"\n",
							 (GINT64_PRINTTYPE)val, (GINT64_PRINTTYPE)prop->data.i.def);
				else
					fprintf (stderr, " unknown  Default %"G_GINT64_FORMAT"\n",
							 (GINT64_PRINTTYPE)prop->data.i.def);
				break;
			}
			case IW_GRAB_DOUBLE: {
				double min = prop->data.d.min;
				double max = prop->data.d.max;
				double val = prop->data.d.value;
				BOOL ok = val >= min && val <= max;
				char formatOk[40] = " [%.2f - %.2f] %.2f  Default %.2f\n";
				char formatErr[40] = " [%.2f - %.2f] unknown  Default %.2f\n";
				char *format = formatOk;
				if (!ok) format = formatErr;

				if (min < -999999999 || min > 999999999) {
					format[4] = '6';
					format[5] = 'e';
				} else if (ABS(min) > 999) {
					format[4] = '0';
				}
				if (max < -999999999 || max > 999999999) {
					format[11] = '6';
					format[12] = 'e';
				} else if (ABS(max) > 999) {
					format[11] = '0';
				}
				if (val < -999999999 || val > 999999999) {
					formatOk[17] = '6';
					formatOk[18] = 'e';
				}
				if (prop->data.d.def < -999999999 || prop->data.d.def > 999999999) {
					formatOk[31] = '6';
					formatOk[32] = 'e';
					formatErr[35] = '6';
					formatErr[36] = 'e';
				}
				if (ok)
					fprintf (stderr, format, min, max, val, prop->data.d.def);
				else
					fprintf (stderr, format, min, max, prop->data.d.def);
				break;
			}
			case IW_GRAB_ENUM: {
				int val = prop->data.e.value;
				fprintf (stderr, " [");
				for (e = 0; prop->data.e.enums[e].name; e++) {
					if (e > 0)
						fprintf (stderr, " | ");
					if (e == val)
						fprintf (stderr, "*");
					fprintf (stderr, "%s", prop->data.e.enums[e].name);
				}
				fprintf (stderr, "]");
				if (prop->data.e.def >= 0 && prop->data.e.def <= prop->data.e.max)
					fprintf (stderr, "  Default %s\n", prop->data.e.enums[prop->data.e.def].name);
				else
					fprintf (stderr, "\n");
				break;
			}
			case IW_GRAB_STRING:
				fprintf (stderr, " \"%s\"  Default \"%s\"\n",
						 prop->data.s.value ? prop->data.s.value : "",
						 prop->data.s.def ? prop->data.s.def : "");
				break;
			default:
				fprintf (stderr, " unknown\n");
				break;
		}
		if (prop->description)
			fprintf (stderr, "\t%*s%s\n", indent*4+4, "", prop->description);
	}
}

int AVcap_open (VideoDevData *dev, char *devOptions,
				int subSample, VideoStandard vidSignal)
{
	int driver;
	for (driver=1; AVdriver[driver].id; driver++) {
		if (AVdriver[driver].open(dev, devOptions, subSample, vidSignal)) {
			dev->capData->driver = driver;
			return TRUE;
		}
		if (dev->capData) {
			dev->capData->driver = driver;
			AVcap_close (dev);
		}
	}
	return FALSE;
}

/*********************************************************************
  Apply all properties from args, which is a list of iwGrabControl
  and its arguments. args must be finished with IW_GRAB_LAST.
  reinit==TRUE: Image format related settings from VideoDevData
                have changed so that memory for the grabbed images
                must be newly allocated.
*********************************************************************/
iwGrabStatus AVcap_set_properties (VideoDevData *dev, va_list *args, BOOL *reinit)
{
	iwGrabStatus ret = IW_GRAB_STATUS_UNSUPPORTED;
	*reinit = FALSE;
	if (AVdriver[((AVcapData*)dev->capData)->driver].setProp) {
		while (1) {
			iwGrabControl ctrl = va_arg(*args, int);
			if (ctrl == IW_GRAB_LAST) break;

			ret = AVdriver[((AVcapData*)dev->capData)->driver].setProp (dev, ctrl, args, reinit);
		}
	}
	return ret;
}

void AVcap_close (VideoDevData *dev)
{
	AVcapData *cap = dev->capData;

	if (!cap) return;

	if (cap->prop) {
		int p, e;
		for (p = 0; p < cap->prop_cnt; p++) {
			iwGrabProperty *prop = &cap->prop[p];
			if (prop->name)
				free (prop->name);
			if (prop->description)
				free (prop->description);
			if (prop->type == IW_GRAB_ENUM && prop->data.e.enums) {
				for (e = 0; prop->data.e.enums[e].name; e++)
					free (prop->data.e.enums[e].name);
				free (prop->data.e.enums);
			}
		}
		free (cap->prop);
		cap->prop = NULL;
	}

	AVdriver[((AVcapData*)dev->capData)->driver].close (cap);
	dev->capData = NULL;
}

int AVcap_capture (VideoDevData *dev, iwImage *img, struct timeval *time)
{
	return AVdriver[((AVcapData*)dev->capData)->driver].capture
		(dev->capData, img, time);
}
