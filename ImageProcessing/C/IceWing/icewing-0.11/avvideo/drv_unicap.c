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

#ifdef WITH_UNICAP

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unicap.h>
#include <unicap_status.h>

#include "tools/tools.h"
#include "gui/Gtools.h"
#include "AVColor.h"
#include "AVOptions.h"
#include "drv_unicap.h"

#define Udebug(x)		if (data->debug) fprintf x
#define BUFFER_CNT		2

typedef struct uniData {
	AVcapData data;
	AVcapIP ip;				/* Stereo / Bayer handling */
	unicap_device_t device;
	unicap_handle_t handle;
	unicap_format_t format;
	unicap_data_buffer_t buffer[BUFFER_CNT];

	BOOL v4l1;
	int debug;
} uniData;

/*********************************************************************
  Get all available properties of device data->handle.
*********************************************************************/
static void properties_get (uniData *data)
{
	AVcapData *capData = (AVcapData*)data;
	char buffer[20];
	int pnum, i;
	double min, max;
	unicap_property_t p;

	for (pnum = 0;
		 SUCCESS (unicap_enumerate_properties (data->handle, NULL, &p, pnum));
		 pnum++) {

		unicap_get_property (data->handle, &p);
		switch (p.type) {
			case UNICAP_PROPERTY_TYPE_RANGE:
				AVCap_prop_add_double (capData, p.identifier, p.value,
									   p.range.min, p.range.max, p.range.min, pnum);
				break;
			case UNICAP_PROPERTY_TYPE_MENU:
				AVCap_prop_add_enum (capData, p.identifier, 0, 0, pnum);
				for (i=0; i<p.menu.menu_item_count; i++)
					AVCap_prop_append_enum (capData, p.menu.menu_items[i],
											!strcmp(p.menu.menu_items[i], p.menu_item), i);
				break;
			case UNICAP_PROPERTY_TYPE_VALUE_LIST:
				AVCap_prop_add_enum (capData, p.identifier, 0, 0, pnum);

				min = max = p.value_list.values[0];
				for (i=1; i<p.value_list.value_count; i++) {
					if (p.value_list.values[i] < min) min = p.value_list.values[i];
					if (p.value_list.values[i] > max) max = p.value_list.values[i];
				}

				for (i=0; i<p.value_list.value_count; i++) {
					if (max - min > 999)
						sprintf (buffer, "%.0f", p.value_list.values[i]);
					else
						sprintf (buffer, "%.2f", p.value_list.values[i]);
					AVCap_prop_append_enum (capData, buffer,
											iw_isequal(p.value_list.values[i], p.value),
											*(gint64*)&p.value_list.values[i]);
				}
				break;
			default:
				Udebug ((stderr, "unicap: Unsupported property %d(%s), type %d",
						 pnum, p.identifier, p.type));
				break;
		}
	}
}

/*********************************************************************
  Get values of all available properties of device data->handle.
*********************************************************************/
static void properties_get_values (uniData *data)
{
	AVcapData *capData = (AVcapData*)data;
	unicap_property_t p;
	int pnum, i;

	for (pnum=0; pnum < capData->prop_cnt; pnum++) {
		iwGrabProperty *prop = capData->prop+pnum;
		if (prop->type <= IW_GRAB_COMMAND)
			continue;
		if (   SUCCESS(unicap_enumerate_properties(data->handle, NULL, &p, prop->priv))
			&& SUCCESS(unicap_get_property (data->handle, &p))) {
			switch (p.type) {
				case UNICAP_PROPERTY_TYPE_RANGE:
					prop->data.d.value = p.value;
					break;
				case UNICAP_PROPERTY_TYPE_MENU:
					for (i=0; i<p.menu.menu_item_count; i++) {
						if (!strcmp(p.menu.menu_items[i], p.menu_item)) {
							prop->data.e.value = i;
							break;
						}
					}
					break;
				case UNICAP_PROPERTY_TYPE_VALUE_LIST:
					for (i=0; i<p.value_list.value_count; i++) {
						if (iw_isequal(p.value_list.values[i],
									   *(double*)&prop->data.e.enums[prop->data.e.value].priv)) {
							prop->data.e.value = i;
							break;
						}
					}
					break;
				default:
					Udebug ((stderr, "unicap: Unsupported property %d(%s), type %d",
							 pnum, p.identifier, p.type));
					break;
			}
		} else
			AVcap_error ("unicap: Failed to get property %d(%s)", pnum, prop->name);
	}
}

/*********************************************************************
  Check for "propX" options in devOptions and set the corresponding
  property of data->handle. Return IW_GRAB_STATUS_OK on success.
*********************************************************************/
static iwGrabStatus properties_set (uniData *data, char *devOptions)
{
	unicap_property_t p;
	iwGrabStatus status = IW_GRAB_STATUS_OK;
	AVcapData *capData = (AVcapData*)data;
	char propArg[100], propVal[200];
	char *optPos;

	optPos = devOptions;
	while ((optPos = AVcap_optstr_lookup_prefix(optPos, "prop", propArg, sizeof(propArg),
												propVal, sizeof(propVal)))) {
		iwGrabProperty *prop;
		int propIdx = AVCap_prop_lookup (capData, propArg);
		if (propIdx < 0) {
			AVcap_warning ("unicap: No property '%s' available", propArg);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}
		prop = &capData->prop[propIdx];
		if (!AVCap_prop_parse (prop, prop, propVal)) {
			AVcap_warning ("unicap: Failed to parse value for property %d(%s) from '%s'",
						   propIdx, prop->name, propVal);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}

		unicap_void_property (&p);
		if (!SUCCESS (unicap_enumerate_properties (data->handle, NULL, &p, prop->priv))) {
			AVcap_error ("unicap: Failed to get property %d(%s)", propIdx, prop->name);
			status = IW_GRAB_STATUS_ERR;
			continue;
		}
		if (p.type == UNICAP_PROPERTY_TYPE_RANGE) {
			p.value = prop->data.d.value;
		} else if (p.type == UNICAP_PROPERTY_TYPE_MENU) {
			strncpy (p.menu_item, prop->data.e.enums[prop->data.e.value].name, sizeof(p.menu_item));
			p.menu_item[sizeof(p.menu_item)-1] = '\0';
		} else if (p.type == UNICAP_PROPERTY_TYPE_VALUE_LIST) {
			p.value = *(double*)&prop->data.e.enums[prop->data.e.value].priv;
		}
		if (!SUCCESS (unicap_set_property (data->handle, &p))) {
			AVcap_error ("unicap: Failed to set property %d(%s) to '%s'",
						 propIdx, prop->name, propVal);
			status = IW_GRAB_STATUS_ERR;
			continue;
		}
		status = IW_GRAB_STATUS_OK;
		Udebug ((stderr, "unicap: Set property %d(%s) to value '%s'\n",
				 propIdx, prop->name, propVal));
	}
	return status;
}

int AVuni_open (VideoDevData *dev, char *devOptions,
				int subSample, VideoStandard vidSignal)
{
	int i, j;
	int device_num = 0, format_num = 0;
	uniData *data;
	unicap_device_t *d;
	unicap_format_t *f;

	/* Grabbing method supported ? */
	if (vidSignal != AV_UNICAP) return FALSE;

	data = calloc (1, sizeof(uniData));
	dev->capData = (AVcapData*)data;

	data->ip.stereo = AV_STEREO_NONE;
	data->ip.bayer = IW_IMG_BAYER_NONE;
	data->ip.pattern = IW_IMG_BAYER_RGGB;

	if (devOptions) {
		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "unicap driver V0.2 by Frank Loemker\n"
					 "usage: device=val:format=val:propX=val:propY=val:bayer=method:\n"
					 "       pattern=RGGB|BGGR|GRBG|GBRG:stereo=raw|deinter:debug\n"
					 "device   device number [0...], default: %d\n"
					 "format   format number [0...], default: %d\n"
					 "propX    set property X ([0...] or it's name) to val. If \"=val\" is not given\n"
					 "         use the properties default value.\n"
					 AVCAP_IP_HELP
					 "debug    output debugging/settings information, default: off\n\n",
					 device_num, format_num);
			return FALSE;
		}
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		AVcap_optstr_get (devOptions, "device", "%d", &device_num);
		AVcap_optstr_get (devOptions, "format", "%d", &format_num);

		if (!AVcap_parse_ip ("unicap", devOptions, &data->ip))
			return FALSE;
	}

	d = &data->device;
	f = &data->format;
	unicap_void_device (d);
	unicap_void_format (f);

	/* Check available devices and open one */
	if (data->debug) {
		fprintf (stderr, "unicap: Available devices:\n");
		for (i = 0; SUCCESS (unicap_enumerate_devices(NULL, d, i) ); i++)
			fprintf (stderr, "\t%i: %s [%s]\n", i, d->identifier, d->device);
	}
	if (!SUCCESS (unicap_enumerate_devices (NULL, d, device_num))) {
		AVcap_error ("unicap: Device %d not found", device_num);
		return FALSE;
	}
	Udebug ((stderr, "unicap: Using device %d: %s [%s]\n",
			 device_num, d->identifier, d->device));

	if (!SUCCESS (unicap_open (&data->handle, d))) {
		AVcap_error	("unicap: Failed to open device: %s", d->identifier);
		return FALSE;
	}
	if (strstr (d->cpi_layer, "v4l."))
		data->v4l1 = TRUE;

	/* Set given properties and show available ones */
	properties_get (data);
	properties_set (data, devOptions);
	properties_get_values (data);
	if (data->debug)
		AVCap_prop_print ("unicap", dev->capData);

	/* Check available formats and select one */
	if (data->debug) {
		fprintf (stderr, "unicap: Available formats:\n");
		for (i = 0; SUCCESS (unicap_enumerate_formats (data->handle, NULL, f, i)); i++) {
			fprintf (stderr, "\t%2d: %s %dx%d",
					 i, f->identifier, f->size.width, f->size.height);
			if (f->size_count > 0) {
				fprintf (stderr, " [");
				for (j = 0; j < f->size_count; j++) {
					fprintf (stderr, "%dx%d", f->sizes[j].width, f->sizes[j].height);
					if (j < f->size_count-1)
						fprintf (stderr, ", ");
				}
				fprintf (stderr, "]");
			}
			fprintf (stderr, "\n");
		}
	}
	if (!SUCCESS (unicap_enumerate_formats (data->handle, NULL, f, format_num))) {
		AVcap_error ("unicap: Failed to get video format %d", format_num);
		return FALSE;
	}

	if (f->size_count > 0) {
		dev->f_width = f->sizes[f->size_count-1].width;
		dev->f_height = f->sizes[f->size_count-1].height;

		i = f->size_count-subSample;
		if (i >= f->size_count)
			i = f->size_count-1;
		else if (i < 0)
			i = 0;
		f->size.width = f->sizes[i].width;
		f->size.height = f->sizes[i].height;
	} else {
		dev->f_width = f->size.width;
		dev->f_height = f->size.height;

		if (subSample > 0) {
			f->size.width = f->size.width/subSample;
			f->size.height = f->size.height/subSample;
		}
		f->size.width -= f->size.width%16;
		f->size.height -= f->size.height%16;
	}
	if (!SUCCESS (unicap_set_format (data->handle, f))) {
		AVcap_error ("unicap: Failed to set video format %d", format_num);
		return FALSE;
	}
	Udebug ((stderr, "unicap: Using format %d: %s [%dx%d]\n",
			 format_num, f->identifier, f->size.width, f->size.height));

	if (!SUCCESS (unicap_start_capture (data->handle))) {
		AVcap_error ("unicap: Failed to start capture on device: %s", d->identifier);
		return FALSE;
	}

	/* Queue some buffers */
	for (i=0; i<BUFFER_CNT; i++) {
		/* FIXME Workaround unicap bugs? */
		data->buffer[i].buffer_size =
			f->size.width * f->size.height * ((f->bpp+3) & ~3) / 8;
		if (data->buffer[i].buffer_size < f->buffer_size)
			data->buffer[i].buffer_size = f->buffer_size;
		if (!(data->buffer[i].data = malloc (data->buffer[i].buffer_size))) {
			AVcap_error ("unicap: Failed to allocate %ld bytes buffer memory",
						 (long)data->buffer[i].buffer_size);
			return FALSE;
		}
		if (!SUCCESS (unicap_queue_buffer (data->handle, &data->buffer[i]))) {
			AVcap_error ("unicap: Failed to queue a buffer on device: %s", d->identifier);
			return FALSE;
		}
	}

	dev->width = f->size.width;
	dev->height = f->size.height;
	if (f->fourcc == IW_FOURCC('Y','8','0','0') ||
		f->fourcc == IW_FOURCC('Y','1','6','0') ||
		f->fourcc == IW_FOURCC('G','R','E','Y') ||
		f->fourcc == IW_FOURCC('H','I','2','4') ||
		f->fourcc == IW_FOURCC('H','2','4','0'))
		dev->planes = 1;
	else
		dev->planes = 3;
	if (f->fourcc == IW_FOURCC('Y','1','6','0'))
		dev->type = IW_16U;
	else
		dev->type = IW_8U;

	if (data->ip.stereo != AV_STEREO_NONE && f->bpp != 16) {
		AVcap_warning ("unicap: Stereo decoding only supported for 2-byte modes\n"
					   "        -> deactivating stereo decoding");
		data->ip.stereo = AV_STEREO_NONE;
	}
	if (data->ip.stereo == AV_STEREO_NONE
		&& data->ip.bayer != IW_IMG_BAYER_NONE && dev->planes != 1) {
		AVcap_warning ("unicap: Bayer decoding only supported for 8/16 Bit mono modes\n"
					   "        -> deactivating bayer decoding");
		data->ip.bayer = IW_IMG_BAYER_NONE;
	}
	if (data->ip.bayer != IW_IMG_BAYER_NONE)
		dev->planes = 3;
	else if (data->ip.stereo != AV_STEREO_NONE)
		dev->planes = 1;
	if (data->ip.stereo != AV_STEREO_NONE)
		dev->type = IW_8U;

	if (!AVcap_init_ip ("unicap", dev, &data->ip, FALSE))
		return FALSE;

	return TRUE;
}

void AVuni_close (void *capData)
{
	int i;
	uniData *data = capData;
	unicap_data_buffer_t *buffer;

	if (data->handle) {
		if (!SUCCESS (unicap_stop_capture (data->handle)))
			AVcap_warning ("unicap: Failed to stop capture on device: %s",
						   data->device.identifier);
		while (SUCCESS (unicap_dequeue_buffer (data->handle, &buffer)))
			/* empty */;
		if (!SUCCESS (unicap_close (data->handle)))
			AVcap_warning ("unicap: Failed to close the device: %s",
						   data->device.identifier);
		data->handle = NULL;
	}
	for (i=0; i<BUFFER_CNT; i++) {
		if (data->buffer[i].data) {
			free (data->buffer[i].data);
			data->buffer[i].data = NULL;
		}
	}
	if (data->ip.buffer) {
		free (data->ip.buffer);
		data->ip.buffer = NULL;
	}

	free (data);
}

iwGrabStatus AVuni_set_property (VideoDevData *dev, iwGrabControl ctrl, va_list *args, BOOL *reinit)
{
	iwGrabStatus ret = IW_GRAB_STATUS_OK;
	uniData *data = (uniData*)dev->capData;

	if (!data->handle) return IW_GRAB_STATUS_NOTOPEN;

	switch (ctrl) {
		case IW_GRAB_GET_PROP:
		case IW_GRAB_GET_PROPVALUES: {
			iwGrabProperty **prop = va_arg (*args, iwGrabProperty**);
			if (prop) {
				*prop = dev->capData->prop;
				if (ctrl == IW_GRAB_GET_PROPVALUES)
					properties_get_values (data);
			} else
				ret = IW_GRAB_STATUS_ARGUMENT;
			break;
		}
		case IW_GRAB_SET_PROP: {
			char *props = va_arg (*args, char*);
			ret = properties_set (data, props);
			break;
		}
		default:
			ret = IW_GRAB_STATUS_UNSUPPORTED;
			break;
	}
	return ret;
}

int AVuni_capture (void *capData, iwImage *img, struct timeval *time)
{
	uniData *data = capData;
	unicap_data_buffer_t *buffer;
	struct timeval ctime;
	unsigned int decompose;
	guchar *s;
	int cnt;

	gettimeofday (&ctime, NULL);

	cnt = 0;
	do {
		if (cnt > 0) {
			if (!SUCCESS (unicap_queue_buffer (data->handle, buffer))) {
				AVcap_error ("unicap: Failed to queue a buffer on device: %s",
							 data->device.identifier);
				return FALSE;
			}
		}
		if (!SUCCESS (unicap_wait_buffer (data->handle, &buffer))) {
			AVcap_error ("unicap: Failed to wait for buffer on device: %s",
						 data->device.identifier);
			return FALSE;
		}
		if (buffer->fill_time.tv_sec != 0)
			*time = buffer->fill_time;
		else
			*time = ctime;
		cnt++;
	} while (ABS(IW_TIME_DIFF(ctime, *time)) > 400 && cnt <= BUFFER_CNT);
	if (cnt > 1)
		Udebug ((stderr, "unicap: Skipped %d buffers...\n", cnt-1));

	s = (guchar *)buffer->data;
	if (data->ip.stereo == AV_STEREO_DEINTER) {
		iw_img_stereo_decode (s, data->ip.buffer, IW_8U,
							  buffer->format.size.width * buffer->format.size.height);
		s = data->ip.buffer;
		decompose = IW_FOURCC('Y','8','0','0');
	} else if (data->ip.stereo == AV_STEREO_RAW) {
		decompose = IW_FOURCC('Y','8','0','0');
	} else
		decompose = buffer->format.fourcc;

	switch (decompose) {
		case IW_FOURCC('Y','8','0','0'):
		case IW_FOURCC('G','R','E','Y'):
		case IW_FOURCC('H','I','2','4'):
		case IW_FOURCC('H','2','4','0'):
			if (data->ip.bayer == IW_IMG_BAYER_NONE) {
				img->ctab = IW_GRAY;
				memcpy (img->data[0], s, img->width*img->height);
			} else {
				img->ctab = IW_RGB;
				iw_img_bayer_decode (s, img, data->ip.bayer, data->ip.pattern);
			}
			break;
		case IW_FOURCC('Y','1','6','0'):
			AVcap_mono16_ip (&data->ip, img, s,
							 buffer->format.size.width * buffer->format.size.height,
							 G_BYTE_ORDER != G_BIG_ENDIAN);
			break;
		case IW_FOURCC('R','G','B','O'):
		case IW_FOURCC('R','G','B','5'):
			av_color_bgr15 (img, s);
			img->ctab = IW_RGB;
			break;
		case IW_FOURCC('R','G','B','Q'):
			av_color_bgr15x (img, s);
			img->ctab = IW_RGB;
			break;
		case IW_FOURCC('R','G','B','P'):
		case IW_FOURCC('R','G','B','6'):
			av_color_bgr16 (img, s);
			img->ctab = IW_RGB;
			break;
		case IW_FOURCC('R','G','B','R'):
			av_color_bgr16x (img, s);
			img->ctab = IW_RGB;
			break;
		case IW_FOURCC('B','G','R','3'):
			av_color_rgb24 (img, s, 2, 1, 0);
			img->ctab = IW_RGB;
			break;
		case IW_FOURCC('R','G','B','3'):
			if (data->v4l1)
				av_color_rgb24 (img, s, 2, 1, 0);
			else
				av_color_rgb24 (img, s, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case IW_FOURCC('B','G','R','4'):
			av_color_bgr32 (img, s);
			img->ctab = IW_RGB;
			break;
		case IW_FOURCC('R','G','B','4'):
			if (data->v4l1)
				av_color_bgr32 (img, s);
			else
				av_color_rgb32 (img, s);
			img->ctab = IW_RGB;
			break;

		case IW_FOURCC('Y','4','4','4'):
			av_color_rgb24 (img, s, 1, 0, 2);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('Y','4','2','2'):
		case IW_FOURCC('Y','U','Y','V'):
			av_color_yuyv (img, s, 0);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('U','Y','V','Y'):
			av_color_yuyv (img, s, 1);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('4','2','2','P'):
			av_color_yuv422P (img, s);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('Y','U','1','2'):
			av_color_yuv420 (img, s, 1);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('Y','V','1','2'):
			av_color_yuv420 (img, s, 2);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('Y','4','1','1'):
			av_color_uyyvyy (img, s);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('Y','4','1','P'):
			av_color_yuv41P (img, s);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('4','1','1','P'):
			av_color_yuv411P (img, s);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('Y','U','V','9'):
			av_color_yuv410 (img, s, 1);
			img->ctab = IW_YUV;
			break;
		case IW_FOURCC('Y','V','U','9'):
			av_color_yuv410 (img, s, 2);
			img->ctab = IW_YUV;
			break;
		default:
			AVcap_warning ("unicap: FOURCC %d not implemented for conversion",
						   buffer->format.fourcc);
			break;
	}

	if (!SUCCESS (unicap_queue_buffer (data->handle, buffer))) {
		AVcap_error ("unicap: Failed to queue a buffer on device: %s", data->device.identifier);
		return FALSE;
	}

	return TRUE;
}

#endif	/* WITH_UNICAP */
