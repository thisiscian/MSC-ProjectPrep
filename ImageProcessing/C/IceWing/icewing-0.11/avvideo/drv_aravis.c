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

#ifdef WITH_ARAVIS

#include <stdio.h>
#include <string.h>
#include <arv.h>

#include "AVColor.h"
#include "AVOptions.h"
#include "drv_aravis.h"

#define MAX_REQUESTS	10
#define Adebug(x)		if (data->debug) fprintf x

static struct {
	ArvPixelFormat format;
	int bpp;
	int planes;
	iwImgBayerPattern pattern;
} formats[] = {
	{ARV_PIXEL_FORMAT_MONO_8,          8, 1, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_MONO_8_SIGNED,   8, 1, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_MONO_10,        16, 1, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_MONO_10_PACKED, 12, 1, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_MONO_12,        16, 1, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_MONO_12_PACKED, 12, 1, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_MONO_14,        16, 1, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_MONO_16,        16, 1, IW_IMG_BAYER_AUTO},

	{ARV_PIXEL_FORMAT_BAYER_GR_8,      8, 1, IW_IMG_BAYER_GRBG},
	{ARV_PIXEL_FORMAT_BAYER_RG_8,      8, 1, IW_IMG_BAYER_RGGB},
	{ARV_PIXEL_FORMAT_BAYER_GB_8,      8, 1, IW_IMG_BAYER_GBRG},
	{ARV_PIXEL_FORMAT_BAYER_BG_8,      8, 1, IW_IMG_BAYER_BGGR},
	{ARV_PIXEL_FORMAT_BAYER_GR_10,    16, 1, IW_IMG_BAYER_GRBG},
	{ARV_PIXEL_FORMAT_BAYER_RG_10,    16, 1, IW_IMG_BAYER_RGGB},
	{ARV_PIXEL_FORMAT_BAYER_GB_10,    16, 1, IW_IMG_BAYER_GBRG},
	{ARV_PIXEL_FORMAT_BAYER_BG_10,    16, 1, IW_IMG_BAYER_BGGR},
	{ARV_PIXEL_FORMAT_BAYER_GR_12,    16, 1, IW_IMG_BAYER_GRBG},
	{ARV_PIXEL_FORMAT_BAYER_RG_12,    16, 1, IW_IMG_BAYER_RGGB},
	{ARV_PIXEL_FORMAT_BAYER_GB_12,    16, 1, IW_IMG_BAYER_GBRG},
	{ARV_PIXEL_FORMAT_BAYER_BG_12,    16, 1, IW_IMG_BAYER_BGGR},
	{ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED, 12, 1, IW_IMG_BAYER_BGGR},

	{ARV_PIXEL_FORMAT_RGB_8_PACKED,   24, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_BGR_8_PACKED,   24, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_RGBA_8_PACKED,  32, 4, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_BGRA_8_PACKED,  32, 4, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_RGB_10_PACKED,  48, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_BGR_10_PACKED,  48, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_RGB_12_PACKED,  48, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_BGR_12_PACKED,  48, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_YUV_411_PACKED, 12, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_YUV_422_PACKED, 16, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_YUV_444_PACKED, 24, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_RGB_8_PLANAR,   24, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_RGB_10_PLANAR,  48, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_RGB_12_PLANAR,  48, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_RGB_16_PLANAR,  48, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED, 16, 3, IW_IMG_BAYER_AUTO},

	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_GR_12_PACKED,  12, 1, IW_IMG_BAYER_GRBG},
	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_RG_12_PACKED,  12, 1, IW_IMG_BAYER_RGGB},
	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_GB_12_PACKED,  12, 1, IW_IMG_BAYER_GBRG},
	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_BG_12_PACKED,  12, 1, IW_IMG_BAYER_BGGR},
	{ARV_PIXEL_FORMAT_CUSTOM_YUV_422_YUYV_PACKED, 16, 3, IW_IMG_BAYER_AUTO},
	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_GR_16,         16, 1, IW_IMG_BAYER_GRBG},
	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_RG_16,         16, 1, IW_IMG_BAYER_RGGB},
	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_GB_16,         16, 1, IW_IMG_BAYER_GBRG},
	{ARV_PIXEL_FORMAT_CUSTOM_BAYER_BG_16,         16, 1, IW_IMG_BAYER_BGGR},
	{0, 0, 0, IW_IMG_BAYER_AUTO}
};

#define ERROR(res, x) if (!(res)) {	\
		AVcap_warning x;			\
		return FALSE;				\
	}

typedef struct aravisData {
	AVcapData data;
	AVcapIP ip;				/* Stereo / Bayer handling */

	ArvCamera *camera;
	ArvStream *stream;
	int debug;
} aravisData;

#ifdef STREAM_DEBUG
static void stream_callback (void *data, ArvStreamCallbackType type, ArvBuffer *buffer)
{
	/* VideoDevData *dev = data; */
	printf ("Callback %d %p", type, buffer);
	if (buffer)
		printf (" status %d id %d\n", buffer->status, buffer->frame_id);
	else
		printf ("\n");
}
#endif

static void add_desc (iwGrabProperty *prop, const char *desc)
{
	if (desc) prop->description = strdup (desc);
}

/*********************************************************************
  Recursively get all available properties starting with 'name' of the
  the device 'genicam'.
*********************************************************************/
static void properties_get (aravisData *data, ArvGc *genicam, const char *name)
{
	AVcapData *capData = (AVcapData*)data;
	const GSList *list;
	const GSList *pos;
	const char *desc;
	ArvGcFeatureNode *fnode;
	ArvGcNode *node = arv_gc_get_node (genicam, name);

	if (!ARV_IS_GC_FEATURE_NODE (node)) return;
	fnode = ARV_GC_FEATURE_NODE(node);
	if (!arv_gc_feature_node_is_implemented (fnode, NULL)) return;

	desc = arv_gc_feature_node_get_description (fnode, NULL);

	if (ARV_IS_GC_CATEGORY (node)) {
		list = arv_gc_category_get_features (ARV_GC_CATEGORY(node));
		if (strcmp("Root", name)) {
			add_desc (AVCap_prop_add_category (capData, name, 0), desc);
			for (pos = list; pos; pos = pos->next)
				properties_get (data, genicam, pos->data);
			AVCap_prop_add_category (capData, "<", 0);
		} else {
			for (pos = list; pos; pos = pos->next)
				properties_get (data, genicam, pos->data);
		}
	} else if (ARV_IS_GC_COMMAND (node)) {
		add_desc (AVCap_prop_add_command (capData, name, IWPTR_TO_INT64(node)), desc);
	} else if (ARV_IS_GC_BOOLEAN (node)) {
		add_desc (AVCap_prop_add_bool (capData, name,
									   arv_gc_boolean_get_value(ARV_GC_BOOLEAN(node), NULL),
									   FALSE, IWPTR_TO_INT64(node)),
				  desc);
	} else if (ARV_IS_GC_INTEGER_NODE (node)) {
		int min = arv_gc_integer_get_min(ARV_GC_INTEGER(node), NULL);
		int max = arv_gc_integer_get_max(ARV_GC_INTEGER(node), NULL);
		add_desc (AVCap_prop_add_int (capData, name,
									  arv_gc_integer_get_value(ARV_GC_INTEGER(node), NULL),
									  min, max, (min+max)/2, IWPTR_TO_INT64(node)),
				  desc);
	} else if (ARV_IS_GC_FLOAT_NODE (node)) {
		double min = arv_gc_float_get_min(ARV_GC_FLOAT(node), NULL);
		double max = arv_gc_float_get_max(ARV_GC_FLOAT(node), NULL);
		double def = 0;
		if (max > 1e20) {
			if (min > 0) def = min;
		} else if (min < -1e20) {
			if (max < 0) def = max;
		} else
			def = (min+max)/2;
		add_desc (AVCap_prop_add_double (capData, name,
										 arv_gc_float_get_value(ARV_GC_FLOAT(node), NULL),
										 min, max, def, IWPTR_TO_INT64(node)),
				  desc);
	} else if (ARV_IS_GC_ENUMERATION (node)) {
		gint64 curVal = arv_gc_enumeration_get_int_value (ARV_GC_ENUMERATION(node), NULL);
		add_desc (AVCap_prop_add_enum (capData, name, 0, 0, IWPTR_TO_INT64(node)), desc);
		list = arv_gc_enumeration_get_entries (ARV_GC_ENUMERATION(node));
		for (pos = list; pos; pos = pos->next) {
			if (arv_gc_feature_node_is_implemented (pos->data, NULL)) {
				gint64 val = arv_gc_enum_entry_get_value(pos->data, NULL);
				AVCap_prop_append_enum (capData, arv_gc_feature_node_get_name(pos->data),
										curVal == val, val);
			}
		}
	} else if (ARV_IS_GC_REGISTER (node)) {
		iwGrabProperty *prop = AVCap_prop_add_string (capData, name,
													  arv_gc_feature_node_get_value_as_string (fnode, NULL),
													  "", IWPTR_TO_INT64(node));
		int len = 28;
		if (desc) len += strlen (desc);
		prop->description = malloc (len);
		sprintf (prop->description, "Addr 0x%"G_GINT64_MODIFIER"x",
				 arv_gc_register_get_address (ARV_GC_REGISTER(node), NULL));
		if (desc) {
			strcat (prop->description, ", ");
			strcat (prop->description, desc);
		}
	} else {
		add_desc (AVCap_prop_add_string (capData, name,
										 arv_gc_feature_node_get_value_as_string (fnode, NULL),
										 "", IWPTR_TO_INT64(node)),
				  desc);
	}
}

/*********************************************************************
  Get values of all available properties of device data->camera.
*********************************************************************/
static void properties_get_values (aravisData *data)
{
	AVcapData *capData = (AVcapData*)data;
	int i;

	/* Get values of all available properties */
	for (i=1; i < capData->prop_cnt; i++) {
		iwGrabProperty *prop = &capData->prop[i];
		if (prop->type <= IW_GRAB_COMMAND)
			continue;
		switch (prop->type) {
			case IW_GRAB_BOOL:
				prop->data.b.value = arv_gc_boolean_get_value (ARV_GC_BOOLEAN(IWINT64_TO_PTR(prop->priv)), NULL);
				break;
			case IW_GRAB_INT:
				prop->data.i.value = arv_gc_integer_get_value (ARV_GC_INTEGER(IWINT64_TO_PTR(prop->priv)), NULL);
				break;
			case IW_GRAB_DOUBLE:
				prop->data.d.value = arv_gc_float_get_value (ARV_GC_FLOAT(IWINT64_TO_PTR(prop->priv)), NULL);
				break;
			case IW_GRAB_ENUM: {
				int val = arv_gc_enumeration_get_int_value (ARV_GC_ENUMERATION(IWINT64_TO_PTR(prop->priv)), NULL);
				int e;
				for (e=0; prop->data.e.enums[e].name; e++) {
					if (prop->data.e.enums[e].priv == val) {
						prop->data.e.value = e;
						break;
					}
				}
				break;
			}
			case IW_GRAB_STRING:
				if (prop->data.s.value) free (prop->data.s.value);
				prop->data.s.value = (char*)arv_gc_feature_node_get_value_as_string
					(ARV_GC_FEATURE_NODE(IWINT64_TO_PTR(prop->priv)), NULL);
				if (prop->data.s.value) prop->data.s.value = strdup (prop->data.s.value);
				break;
			default:
				Adebug ((stderr, "aravis: Unsupported property %d(%s), type %d",
						 i, prop->name, prop->type));
				break;
		}
	}
}

/*********************************************************************
  Check for "propX" options in devOptions and set the corresponding
  property from data->camera. Return IW_GRAB_STATUS_OK on success.
*********************************************************************/
static iwGrabStatus properties_set (aravisData *data, char *devOptions)
{
	ArvDevice *device;
	iwGrabStatus status = IW_GRAB_STATUS_OK;
	AVcapData *capData = (AVcapData*)data;
	char propArg[100], propVal[200];
	char *optPos;

	device = arv_camera_get_device (data->camera);
	ERROR (device, ("aravis: arv_camera_get_device() failed"));

	optPos = devOptions;
	while ((optPos = AVcap_optstr_lookup_prefix(optPos, "prop", propArg, sizeof(propArg),
												propVal, sizeof(propVal)))) {
		iwGrabProperty *prop;
		int propIdx = AVCap_prop_lookup (capData, propArg);

		status = IW_GRAB_STATUS_OK;
		if (propIdx < 0) {
			if (propArg[0] == 'R' && propArg[1] == '[') {
				guint32 address = strtoll(propArg+2, NULL, 0);
				guint32 curVal;
				if (propVal[0])
					arv_device_write_register (device, address, strtoll(propVal, NULL, 0), NULL);
				arv_device_read_register (device, address, &curVal, NULL);
				Adebug ((stderr, "aravis: %sregister 0x%08x = 0x%08x\n",
						 propVal[0]?"Set ":"", address, curVal));
			} else {
				AVcap_warning ("aravis: No property '%s' available", propArg);
				status = IW_GRAB_STATUS_ARGUMENT;
			}
			continue;
		}
		prop = &capData->prop[propIdx];
		if (!AVCap_prop_parse (prop, prop, propVal)) {
			AVcap_warning ("aravis: Failed to parse value for property %d(%s) from '%s'",
						   propIdx, prop->name, propVal);
			status = IW_GRAB_STATUS_ARGUMENT;
			continue;
		}

		switch (prop->type) {
			case IW_GRAB_COMMAND:
				arv_gc_command_execute (ARV_GC_COMMAND(IWINT64_TO_PTR(prop->priv)), NULL);
				Adebug ((stderr, "aravis: %s executed\n", propArg));
				break;
			case IW_GRAB_BOOL:
				arv_gc_boolean_set_value (ARV_GC_BOOLEAN(IWINT64_TO_PTR(prop->priv)), prop->data.b.value, NULL);
				break;
			case IW_GRAB_INT:
				arv_gc_integer_set_value (ARV_GC_INTEGER(IWINT64_TO_PTR(prop->priv)), prop->data.i.value, NULL);
				break;
			case IW_GRAB_DOUBLE:
				arv_gc_float_set_value (ARV_GC_FLOAT(IWINT64_TO_PTR(prop->priv)), prop->data.d.value, NULL);
				break;
			case IW_GRAB_ENUM:
				arv_gc_enumeration_set_int_value (ARV_GC_ENUMERATION(IWINT64_TO_PTR(prop->priv)),
												  prop->data.e.enums[prop->data.e.value].priv, NULL);
				break;
			case IW_GRAB_STRING:
				arv_gc_feature_node_set_value_from_string
					(ARV_GC_FEATURE_NODE(IWINT64_TO_PTR(prop->priv)), propVal, NULL);
				break;
			default: break;
		}

		if (0) {
			status = IW_GRAB_STATUS_ERR;
			AVcap_error ("aravis: Failed to set property %d(%s) to '%s'",
						 propIdx, prop->name, propVal);
		} else if (prop->type != IW_GRAB_COMMAND)
			Adebug ((stderr, "aravis: Set property %d(%s) to value '%s'\n",
					 propIdx, prop->name,
					 arv_gc_feature_node_get_value_as_string
						(ARV_GC_FEATURE_NODE(IWINT64_TO_PTR(prop->priv)), NULL)));
	}
	return status;
}

int AVaravis_open (VideoDevData *dev, char *devOptions,
				   int subSample, VideoStandard vidSignal)
{
#define ARG_LEN         301
	int deviceCount = 0;
	int deviceNum = -1;
	char device[ARG_LEN];
	BOOL listProps = FALSE;
	int i, imgSize;
	char *optPos;
	aravisData *data;
	ArvBuffer *buffer;
	ArvDevice *devicePtr;

	/* Grabbing method supported ? */
	if (vidSignal != AV_ARAVIS) return FALSE;

	data = calloc (1, sizeof(aravisData));
	dev->capData = (AVcapData*)data;

	data->ip.stereo = AV_STEREO_NONE;
	data->ip.bayer = IW_IMG_BAYER_NONE;
	data->ip.pattern = IW_IMG_BAYER_AUTO;
	memset (device, '\0', ARG_LEN);

	if (devOptions) {
		if (AVcap_optstr_lookup (devOptions, "help")) {
			fprintf (stderr, "Aravis driver V0.0 by Frank Loemker\n"
					 "usage: device=val:listProps:propX=val:propY=val:bayer=method:\n"
					 "       pattern=RGGB|BGGR|GRBG|GBRG:stereo=raw|deinter:debug\n"
					 "device   access a camera by it's device name or it's id [0...], default: 0\n"
					 "listProps list all available properties and show additional details\n"
					 "propX    set property X ([0...] or it's name) to val. If \"=val\" is not given\n"
					 "         use the properties default value. If X = \"R[ADR]\", set register ADR\n"
					 "         to val.\n"
					 "         Examples: propWidth=640:propOffsetX:propR[0x123]=240\n"
					 AVCAP_IP_HELP_D("Auto detection / RGGB")
					 "debug    output debugging/settings information, default: off\n\n");
			return FALSE;
		}
		if (AVcap_optstr_lookup (devOptions, "debug"))
			data->debug = TRUE;
		if (!AVcap_parse_ip ("aravis", devOptions, &data->ip))
			return FALSE;
		AVcap_optstr_get (devOptions, "device", "%300c", device);
		if (AVcap_optstr_lookup (devOptions, "listProps"))
			listProps = TRUE;
			
	}

	arv_update_device_list();

	deviceCount = arv_get_n_devices();
	if (deviceCount <= 0) {
		AVcap_warning ("aravis: No devices available");
		return FALSE;
	}
	deviceNum = strtol (device, &optPos, 10);
	if (*optPos == '\0') {
		if (deviceNum < 0 || deviceNum >= deviceCount) {
			AVcap_warning ("aravis: Device %d not available, only device(s) 0..%d available",
						   deviceNum, deviceCount-1);
			return FALSE;
		}
		strcpy (device, arv_get_device_id(deviceNum));
	}
	if (data->debug) {
		fprintf (stderr, "aravis: Available devices:\n");
		for (i = 0; i < deviceCount; i++)
			fprintf (stderr, "\t%d: %s %s\n",
					 i, arv_get_interface_id(i), arv_get_device_id(i));
	}
	Adebug ((stderr, "aravis: %d devices available, using device %s\n",
			 deviceCount, device));

	/* Initialise the camera */
	data->camera = arv_camera_new (device);
	ERROR (data->camera, ("aravis: arv_camera_new() failed"));

	/* Set given properties and show available ones */
	if ((devicePtr = arv_camera_get_device (data->camera))) {
		ArvGc *genicam = arv_device_get_genicam (devicePtr);
		if (genicam)
			properties_get (data, genicam, "Root");
	}
	properties_set (data, devOptions);
	properties_get_values (data);
	if (listProps)
		AVCap_prop_print ("aravis", dev->capData);

	data->stream = arv_camera_create_stream (data->camera,
#ifdef STREAM_DEBUG
											 stream_callback
#else
											 NULL
#endif
											 , dev);
	ERROR (data->stream, ("aravis: arv_camera_create_stream() failed"));

	imgSize = arv_camera_get_payload (data->camera);
	for (i = 0; i < MAX_REQUESTS; i++)
		arv_stream_push_buffer (data->stream, arv_buffer_new(imgSize, NULL));

	arv_camera_set_acquisition_mode (data->camera, ARV_ACQUISITION_MODE_CONTINUOUS);
	arv_camera_start_acquisition (data->camera);

	for (i=0; ;i++) {
		if ((buffer = arv_stream_timeout_pop_buffer (data->stream, 5000*1000))) {
			if (buffer->status == ARV_BUFFER_STATUS_SUCCESS)
				break;
			arv_stream_push_buffer (data->stream, buffer);
		}
		ERROR (i<=MAX_REQUESTS+2, ("aravis: Unable to capture an image"));
	}

	/* Get the destination image properties and allocate image memory */
	{
		int f;
		for (f=0; formats[f].bpp; f++)
			if (formats[f].format == buffer->pixel_format) break;
		ERROR (formats[f].bpp, ("aravis: Unknown pixel format %d", buffer->pixel_format));

		if (data->ip.stereo != AV_STEREO_NONE && formats[f].bpp != 16) {
			AVcap_warning ("aravis: Stereo decoding only supported for 16BPP modes\n"
						   "          -> deactivating stereo decoding");
			data->ip.stereo = AV_STEREO_NONE;
		}
		if (data->ip.stereo == AV_STEREO_NONE && data->ip.bayer != IW_IMG_BAYER_NONE
			&& (formats[f].bpp > 16 || formats[f].planes != 1)) {
			AVcap_warning ("aravis: Bayer decoding only supported for 8/12/16 Bit mono modes\n"
						   "          -> deactivating bayer decoding");
			data->ip.bayer = IW_IMG_BAYER_NONE;
		}
		if (data->ip.pattern == IW_IMG_BAYER_AUTO)
			data->ip.pattern = formats[f].pattern;

		dev->f_width = dev->width = buffer->width;
		dev->f_height = dev->height = buffer->height;
		if (data->ip.bayer != IW_IMG_BAYER_NONE) {
			dev->planes = 3;
		} else if (data->ip.stereo != AV_STEREO_NONE) {
			dev->planes = 1;
		} else {
			dev->planes = formats[f].planes;
			if (dev->planes > 3) dev->planes = 3;
		}

		if (data->ip.stereo != AV_STEREO_NONE) {
			dev->type = IW_8U;
		} else if (formats[f].bpp/formats[f].planes > 8) {
			dev->type = IW_16U;
		} else {
			dev->type = IW_8U;
		}

		if (!AVcap_init_ip ("aravis", dev, &data->ip, FALSE))
			return FALSE;

		Adebug ((stderr, "aravis: Using images %dx%d planes: %d\n",
				 dev->width, dev->height, dev->planes));
	}
	arv_stream_push_buffer (data->stream, buffer);

	return TRUE;
}

void AVaravis_close (void *capData)
{
	aravisData *data = capData;

	if (data->camera) {
		g_object_unref (data->camera);
		data->camera = NULL;
	}
	if (data->stream != NULL) {
		g_object_unref (data->stream);
		data->stream = NULL;
	}

	if (data->ip.buffer) {
		free (data->ip.buffer);
		data->ip.buffer = NULL;
	}

	free (data);
}

iwGrabStatus AVaravis_set_property (VideoDevData *dev, iwGrabControl ctrl, va_list *args, BOOL *reinit)
{
	iwGrabStatus ret = IW_GRAB_STATUS_OK;
	aravisData *data = (aravisData*)dev->capData;

	if (!data->camera) return IW_GRAB_STATUS_NOTOPEN;

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

int AVaravis_capture (void *capData, iwImage *img, struct timeval *time)
{
	aravisData *data = capData;
	guchar *s;
	ArvBuffer *buffer;
	ArvPixelFormat format;
	int i;

	for (i=0; ; i++) {
		if ((buffer = arv_stream_timeout_pop_buffer (data->stream, 5000*1000))) {
			/* printf ("Status %d\n", buffer->status);
			   break; */
			if (buffer->status == ARV_BUFFER_STATUS_SUCCESS)
				break;
			arv_stream_push_buffer (data->stream, buffer);
		}
		ERROR (i<=MAX_REQUESTS+2, ("aravis: Unable to capture an image"));
	}

	time->tv_sec = buffer->timestamp_ns / 1000000000;
	time->tv_usec = buffer->timestamp_ns % 1000000000;

	s = buffer->data;
	format = buffer->pixel_format;
	if (data->ip.stereo == AV_STEREO_DEINTER) {
		iw_img_stereo_decode (s, data->ip.buffer, IW_8U, img->width*img->height/2);
		s = data->ip.buffer;
		format = ARV_PIXEL_FORMAT_MONO_8;
	} else if (data->ip.stereo == AV_STEREO_RAW) {
		format = ARV_PIXEL_FORMAT_MONO_8;
	}

	switch (format) {
		case ARV_PIXEL_FORMAT_MONO_8:
		case ARV_PIXEL_FORMAT_MONO_8_SIGNED:
		case ARV_PIXEL_FORMAT_BAYER_GR_8:
		case ARV_PIXEL_FORMAT_BAYER_RG_8:
		case ARV_PIXEL_FORMAT_BAYER_GB_8:
		case ARV_PIXEL_FORMAT_BAYER_BG_8:
			if (data->ip.bayer == IW_IMG_BAYER_NONE) {
				memcpy (img->data[0], s, img->width*img->height);
				img->ctab = IW_GRAY;
			} else {
				iw_img_bayer_decode (s, img, data->ip.bayer, data->ip.pattern);
				img->ctab = IW_RGB;
			}
			break;
		case ARV_PIXEL_FORMAT_MONO_10:
		case ARV_PIXEL_FORMAT_MONO_12:
		case ARV_PIXEL_FORMAT_MONO_14:
		case ARV_PIXEL_FORMAT_MONO_16:
		case ARV_PIXEL_FORMAT_BAYER_GR_10:
		case ARV_PIXEL_FORMAT_BAYER_RG_10:
		case ARV_PIXEL_FORMAT_BAYER_GB_10:
		case ARV_PIXEL_FORMAT_BAYER_BG_10:
		case ARV_PIXEL_FORMAT_BAYER_GR_12:
		case ARV_PIXEL_FORMAT_BAYER_RG_12:
		case ARV_PIXEL_FORMAT_BAYER_GB_12:
		case ARV_PIXEL_FORMAT_BAYER_BG_12:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_GR_16:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_RG_16:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_GB_16:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_BG_16:
			AVcap_mono16_ip (&data->ip, img, s, img->width*img->height, FALSE);
			break;
		case ARV_PIXEL_FORMAT_MONO_10_PACKED:
		case ARV_PIXEL_FORMAT_MONO_12_PACKED:
		case ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_GR_12_PACKED:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_RG_12_PACKED:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_GB_12_PACKED:
		case ARV_PIXEL_FORMAT_CUSTOM_BAYER_BG_12_PACKED:
			if (data->ip.bayer == IW_IMG_BAYER_NONE) {
				img->ctab = IW_GRAY;
				av_color_mono12 (img->data[0], s, img->width*img->height);
			} else {
				img->ctab = IW_RGB;
				av_color_mono12 (data->ip.buffer, s, img->width*img->height);
				iw_img_bayer_decode (data->ip.buffer, img, data->ip.bayer, data->ip.pattern);
			}
			break;
		case ARV_PIXEL_FORMAT_RGB_8_PACKED:
			av_color_rgb24 (img, s, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case ARV_PIXEL_FORMAT_RGBA_8_PACKED:
			av_color_rgb32 (img, s);
			img->ctab = IW_RGB;
			break;
		case ARV_PIXEL_FORMAT_BGR_8_PACKED:
			av_color_rgb24 (img, s, 2, 1, 0);
			img->ctab = IW_RGB;
			break;
		case ARV_PIXEL_FORMAT_BGRA_8_PACKED:
			av_color_bgr32 (img, s);
			img->ctab = IW_RGB;
			break;
		case ARV_PIXEL_FORMAT_RGB_10_PACKED:
		case ARV_PIXEL_FORMAT_RGB_12_PACKED:
			av_color_rgb24_16 (img, s, FALSE, 0, 1, 2);
			img->ctab = IW_RGB;
			break;
		case ARV_PIXEL_FORMAT_BGR_10_PACKED:
		case ARV_PIXEL_FORMAT_BGR_12_PACKED:
			av_color_rgb24_16 (img, s, FALSE, 2, 1, 0);
			img->ctab = IW_RGB;
			break;

		case ARV_PIXEL_FORMAT_RGB_8_PLANAR:
			av_color_rgb24P (img, s);
			img->ctab = IW_RGB;
			break;
		case ARV_PIXEL_FORMAT_RGB_10_PLANAR:
		case ARV_PIXEL_FORMAT_RGB_12_PLANAR:
		case ARV_PIXEL_FORMAT_RGB_16_PLANAR:
			av_color_rgb24P_16 (img, s);
			img->ctab = IW_RGB;
			break;

		case ARV_PIXEL_FORMAT_YUV_411_PACKED:
			av_color_uyyvyy (img, s);
			img->ctab = IW_YUV;
			break;
		case ARV_PIXEL_FORMAT_YUV_422_PACKED:
			av_color_yuyv (img, s, 1);
			img->ctab = IW_YUV;
			break;
		case ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED:
		case ARV_PIXEL_FORMAT_CUSTOM_YUV_422_YUYV_PACKED:
			av_color_yuyv (img, s, 0);
			img->ctab = IW_YUV;
			break;
		case ARV_PIXEL_FORMAT_YUV_444_PACKED:
			av_color_rgb24 (img, s, 0, 1, 2);
			img->ctab = IW_YUV;
			break;
		default:
			AVcap_warning ("aravis: Pixel format %d not implemented for conversion",
						   buffer->pixel_format);
			break;
	}
	arv_stream_push_buffer (data->stream, buffer);

	return TRUE;
}

#endif /* WITH_ARAVIS */
