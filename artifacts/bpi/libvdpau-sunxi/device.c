/*
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <cedrus/cedrus.h>
#include "vdpau_private.h"

// First entry point into this library.
VdpStatus vdp_device_create_x11(Display *display,
                                int screen,
                                VdpDevice *device,
                                VdpGetProcAddress **get_proc_address)
{
	nx_ini_reload();

	if (!device || !get_proc_address)
		return VDP_STATUS_INVALID_POINTER;

	*get_proc_address = vdp_get_proc_address;

	device_ctx_t *dev = handle_create(sizeof(*dev), device);
	if (!dev)
		return VDP_STATUS_RESOURCES;

	if (screen != -1)
	{
		if (!display)
			return VDP_STATUS_INVALID_POINTER;
		dev->display = XOpenDisplay(XDisplayString(display));
		dev->screen = screen;
		dev->outFullScreenWidthHeight = NULL;
	}
	else
	{
		// Nx extention to VDPAU: Avoid using X11 when screen is -1; return full-screen size via
		// *display, assuming it points to an out-parameter: uint32_t value which receives
		// ((height << 16) | width) when VdpPresentationQueueTargetCreateX11() is called.
		// ATTENTION: This is currently supported only in disp v1.
		dev->display = NULL;
		dev->screen = -1;
		dev->outFullScreenWidthHeight = (uint32_t*) display;
	}

	dev->cedrus = cedrus_open();
	if (!dev->cedrus)
	{
		VDPAU_DBG("Failed to initialize libcedrus");
		if (dev->display)
		    XCloseDisplay(dev->display);
		handle_destroy(*device);
		return VDP_STATUS_ERROR;
	}

	VDPAU_DBG("VE version 0x%04x opened", cedrus_get_ve_version(dev->cedrus));

	char *env_vdpau_osd = getenv("VDPAU_OSD");
	char *env_vdpau_g2d = getenv("VDPAU_DISABLE_G2D");
	if (env_vdpau_osd && strncmp(env_vdpau_osd, "1", 1) == 0)
	{
  		dev->osd_enabled = 1;
	}
	else
	{
		VDPAU_DBG("OSD disabled!");
		return VDP_STATUS_OK;
	}

	if (!env_vdpau_g2d || strncmp(env_vdpau_g2d, "1", 1) !=0)
	{
		dev->g2d_fd = open("/dev/g2d", O_RDWR);
		if (dev->g2d_fd != -1)
		{
			dev->g2d_enabled = 1;
			VDPAU_DBG("OSD enabled, using G2D!");
		}
	}

	if (!dev->g2d_enabled)
		VDPAU_DBG("OSD enabled, using pixman");

	return VDP_STATUS_OK;
}

VdpStatus vdp_device_destroy(VdpDevice device)
{
	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	if (dev->g2d_enabled)
		close(dev->g2d_fd);
	cedrus_close(dev->cedrus);

	if (dev->display)
	    XCloseDisplay(dev->display);

	handle_destroy(device);

	return VDP_STATUS_OK;
}

VdpStatus vdp_preemption_callback_register(VdpDevice device,
                                           VdpPreemptionCallback callback,
                                           void *context)
{
	if (!callback)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	dev->preemption_callback = callback;
	dev->preemption_callback_context = context;

	return VDP_STATUS_OK;
}

VdpStatus vdp_get_proc_address(VdpDevice device_handle,
                               VdpFuncId function_id,
                               void **function_pointer)
{
	if (!function_pointer)
		return VDP_STATUS_INVALID_POINTER;

	// Nx extension: Allow to use vdp_get_proc_address even if device initialization has failed.
	#if 0
		device_ctx_t *device = handle_get(device_handle);
		if (!device)
			return VDP_STATUS_INVALID_HANDLE;
	#endif // 0

	if (function_id < function_count)
	{
		*function_pointer = functions[function_id];
		if (*function_pointer == NULL)
			return VDP_STATUS_INVALID_FUNC_ID;
		else
			return VDP_STATUS_OK;
	}
	else if (function_id == VDP_FUNC_ID_BASE_WINSYS)
	{
		*function_pointer = &log_presentation_queue_target_create_x11;

		return VDP_STATUS_OK;
	}

	return VDP_STATUS_INVALID_FUNC_ID;
}

char const *vdp_get_error_string(VdpStatus status)
{
	switch (status)
	{
	case VDP_STATUS_OK:
		return "No error.";
	case VDP_STATUS_NO_IMPLEMENTATION:
		return "No backend implementation could be loaded.";
	case VDP_STATUS_DISPLAY_PREEMPTED:
		return "The display was preempted, or a fatal error occurred. The application must re-initialize VDPAU.";
	case VDP_STATUS_INVALID_HANDLE:
		return "An invalid handle value was provided.";
	case VDP_STATUS_INVALID_POINTER:
		return "An invalid pointer was provided.";
	case VDP_STATUS_INVALID_CHROMA_TYPE:
		return "An invalid/unsupported VdpChromaType value was supplied.";
	case VDP_STATUS_INVALID_Y_CB_CR_FORMAT:
		return "An invalid/unsupported VdpYCbCrFormat value was supplied.";
	case VDP_STATUS_INVALID_RGBA_FORMAT:
		return "An invalid/unsupported VdpRGBAFormat value was supplied.";
	case VDP_STATUS_INVALID_INDEXED_FORMAT:
		return "An invalid/unsupported VdpIndexedFormat value was supplied.";
	case VDP_STATUS_INVALID_COLOR_STANDARD:
		return "An invalid/unsupported VdpColorStandard value was supplied.";
	case VDP_STATUS_INVALID_COLOR_TABLE_FORMAT:
		return "An invalid/unsupported VdpColorTableFormat value was supplied.";
	case VDP_STATUS_INVALID_BLEND_FACTOR:
		return "An invalid/unsupported VdpOutputSurfaceRenderBlendFactor value was supplied.";
	case VDP_STATUS_INVALID_BLEND_EQUATION:
		return "An invalid/unsupported VdpOutputSurfaceRenderBlendEquation value was supplied.";
	case VDP_STATUS_INVALID_FLAG:
		return "An invalid/unsupported flag value/combination was supplied.";
	case VDP_STATUS_INVALID_DECODER_PROFILE:
		return "An invalid/unsupported VdpDecoderProfile value was supplied.";
	case VDP_STATUS_INVALID_VIDEO_MIXER_FEATURE:
		return "An invalid/unsupported VdpVideoMixerFeature value was supplied.";
	case VDP_STATUS_INVALID_VIDEO_MIXER_PARAMETER:
		return "An invalid/unsupported VdpVideoMixerParameter value was supplied.";
	case VDP_STATUS_INVALID_VIDEO_MIXER_ATTRIBUTE:
		return "An invalid/unsupported VdpVideoMixerAttribute value was supplied.";
	case VDP_STATUS_INVALID_VIDEO_MIXER_PICTURE_STRUCTURE:
		return "An invalid/unsupported VdpVideoMixerPictureStructure value was supplied.";
	case VDP_STATUS_INVALID_FUNC_ID:
		return "An invalid/unsupported VdpFuncId value was supplied.";
	case VDP_STATUS_INVALID_SIZE:
		return "The size of a supplied object does not match the object it is being used with.";
	case VDP_STATUS_INVALID_VALUE:
		return "An invalid/unsupported value was supplied.";
	case VDP_STATUS_INVALID_STRUCT_VERSION:
		return "An invalid/unsupported structure version was specified in a versioned structure.";
	case VDP_STATUS_RESOURCES:
		return "The system does not have enough resources to complete the requested operation at this time.";
	case VDP_STATUS_HANDLE_DEVICE_MISMATCH:
		return "The set of handles supplied are not all related to the same VdpDevice.";
	case VDP_STATUS_ERROR:
		return "A catch-all error, used when no other error code applies.";
	default:
		return "Unknown Error";
   }
}

VdpStatus vdp_get_api_version(uint32_t *api_version)
{
	if (!api_version)
		return VDP_STATUS_INVALID_POINTER;

	*api_version = 1;
	return VDP_STATUS_OK;
}

VdpStatus vdp_get_information_string(char const **information_string)
{
	if (!information_string)
		return VDP_STATUS_INVALID_POINTER;

	*information_string = "sunxi VDPAU Driver";
	return VDP_STATUS_OK;
}

