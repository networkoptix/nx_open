#include "sunxi_disp.h"

/*
 * Copyright (c) 2013-2015 Jens Kuske <jenskuske@gmail.com>
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

#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

#include "kernel-headers/sunxi_disp_ioctl.h"
#include "vdpau_private.h"

struct sunxi_disp_private
{
	struct sunxi_disp pub;

	int fd;
	int video_layer;
	int osd_layer;
	__disp_layer_info_t video_info;
	__disp_layer_info_t osd_info;
	bool video_layer_opened;
};

static void sunxi_disp_close(struct sunxi_disp *sunxi_disp);
static int sunxi_disp_set_video_layer(struct sunxi_disp *sunxi_disp, int x, int y, int width, int height, output_surface_ctx_t *surface);
static void sunxi_disp_close_video_layer(struct sunxi_disp *sunxi_disp);
static int sunxi_disp_set_osd_layer(struct sunxi_disp *sunxi_disp, int x, int y, int width, int height, output_surface_ctx_t *surface);
static void sunxi_disp_close_osd_layer(struct sunxi_disp *sunxi_disp);

static const char* print_ioctl_args(char* target, int len, int fd, int cmd, const uint32_t args[],
	const char* cmdStr, const char* arg0, const char* arg1, const char* arg2, const char* arg3)
{
	char v1[100];
	if (strcmp(arg1, "0") != 0 && strncmp(arg1, "DISP", 4) != 0)
		snprintf(v1, sizeof(v1), "%s: %u", arg1, args[1]);
	else
		snprintf(v1, sizeof(v1), "%s", arg1);

	char v2[100];
	if (arg2[0] == '&')
	{
		switch (cmd)
		{
			case DISP_CMD_LAYER_SET_PARA:
			{
				const __disp_layer_info_t* const t = (const __disp_layer_info_t*) args[2];
				snprintf(v2, sizeof(v2), "{.fb: {.addr: {0x%08X, 0x%08X, 0x%08X}}}",
					t->fb.addr[0], t->fb.addr[1], t->fb.addr[2]);
				break;
			}
			case DISP_CMD_LAYER_SET_FB:
			{
				const __disp_fb_t* const t = (const __disp_fb_t*) args[2];
				snprintf(v2, sizeof(v2), "{.addr: {0x%08X, 0x%08X, 0x%08X}}",
					t->addr[0], t->addr[1], t->addr[2]);
				break;
			}
			default:
				snprintf(v2, sizeof(v2), "%s: %u", arg2, args[2]);
		}
	}
	else if (strcmp(arg2, "0") != 0)
	{
		snprintf(v2, sizeof(v2), "%s: %u", arg2, args[2]);
	}
	else
	{
		snprintf(v2, sizeof(v2), "%s", arg2);
	}

	snprintf(target, len, "/dev/disp, %s, {%s, %s, %s, %s}", cmdStr, arg0, v1, v2, arg3);
	return target;
}

static int disp_ioctl(int fd, int cmd, const uint32_t args[],
	const char* cmdStr, const char* arg0, const char* arg1, const char* arg2, const char* arg3)
{
	int result = ioctl(fd, cmd, args);

	if (ini.enableIoctlOutput || result < 0)
	{
		char s[1000];
		if (result < 0)
		{
			fprintf(stderr, "[VDPAU SUNXI] [disp1] ERROR: ioctl(%s) -> %d: %s\n",
				print_ioctl_args(s, sizeof(s), fd, cmd, args, cmdStr, arg0, arg1, arg2, arg3),
				result, strerror(errno));
		}
		else
		{
			fprintf(stderr, "[VDPAU SUNXI] [disp1] ioctl(%s) -> %d\n",
				print_ioctl_args(s, sizeof(s), fd, cmd, args, cmdStr, arg0, arg1, arg2, arg3),
				result);
		}
	}

	return result;
}

// Conveniency macro for calling ioctl("/dev/disp", ...) with optional logging.
#define DISP(CMD, A0, A1, A2, A3) disp_ioctl(disp->fd, (CMD), \
    (const uint32_t[]) {(uint32_t) (A0), (uint32_t) (A1), (uint32_t) (A2), (uint32_t) (A3)}, \
    #CMD, #A0, #A1, #A2, #A3)

struct sunxi_disp *sunxi_disp_open(int osd_enabled, uint32_t* outWidthHeight)
{
	struct sunxi_disp_private *disp = calloc(1, sizeof(*disp));

	disp->fd = open("/dev/disp", O_RDWR);
	if (disp->fd < 0)
	{
		VDPAU_DBG("[disp1] ERROR: Cannot open /dev/disp");
		goto err_open;
	}

	int version = DISP(DISP_CMD_VERSION, SUNXI_DISP_VERSION, 0, 0, 0);
	if (version < 0)
		goto err_version;
	VDPAU_DBG("[disp1] /dev/disp reported version %d", version);

	disp->video_layer = DISP(DISP_CMD_LAYER_REQUEST, 0, DISP_LAYER_WORK_MODE_SCALER, 0, 0);
	if (disp->video_layer == 0)
		VDPAU_DBG("[disp1] ERROR: Video Layer request returned 0");
    if (disp->video_layer <= 0)
		goto err_video_layer;

	int width = DISP(DISP_CMD_SCN_GET_WIDTH, 0, 0, 0, 0);
	int height = DISP(DISP_CMD_SCN_GET_HEIGHT, 0, 0, 0, 0);
	if (outWidthHeight)
		*outWidthHeight = (height << 16) | width;
	VDPAU_DBG("[disp1] Video Layer %d opened; screen size: %d x %d",
		disp->video_layer, width, height);

	if (osd_enabled)
	{
		DISP(DISP_CMD_LAYER_TOP, 0, disp->video_layer, 0, 0);

		disp->osd_layer = DISP(DISP_CMD_LAYER_REQUEST, 0, DISP_LAYER_WORK_MODE_NORMAL, 0, 0);
		if (disp->osd_layer == 0)
			VDPAU_DBG("[disp1] ERROR: OSD Layer request returned 0");
		if (disp->osd_layer <= 0)
			goto err_osd_layer;

		DISP(DISP_CMD_LAYER_TOP, 0, disp->osd_layer, 0, 0);

		disp->osd_info.pipe = 1;
		disp->osd_info.mode = DISP_LAYER_WORK_MODE_NORMAL;
		disp->osd_info.fb.mode = DISP_MOD_INTERLEAVED;
		disp->osd_info.fb.format = DISP_FORMAT_ARGB8888;
		disp->osd_info.fb.seq = DISP_SEQ_ARGB;
		disp->osd_info.fb.cs_mode = DISP_BT601;
	}
	else
	{
		DISP(DISP_CMD_LAYER_TOP, 0, disp->video_layer, 0, 0);

		if (ini.enableColorKey)
		{
			disp->video_info.pipe = 1;
			disp->video_info.ck_enable = 1;

			__disp_colorkey_t ck;
			ck.ck_max.red = ck.ck_min.red = 0;
			ck.ck_max.green = ck.ck_min.green = 1;
			ck.ck_max.blue = ck.ck_min.blue = 2;
			ck.red_match_rule = 2;
			ck.green_match_rule = 2;
			ck.blue_match_rule = 2;

			DISP(DISP_CMD_SET_COLORKEY, 0, &ck, 0, 0);
		}
	}

	disp->video_info.mode = DISP_LAYER_WORK_MODE_SCALER;
	disp->video_info.fb.cs_mode = DISP_BT601;
	disp->video_info.fb.br_swap = 0;

	disp->pub.close = sunxi_disp_close;
	disp->pub.set_video_layer = sunxi_disp_set_video_layer;
	disp->pub.close_video_layer = sunxi_disp_close_video_layer;
	disp->pub.set_osd_layer = sunxi_disp_set_osd_layer;
	disp->pub.close_osd_layer = sunxi_disp_close_osd_layer;

	return (struct sunxi_disp *)disp;

err_osd_layer:
	DISP(DISP_CMD_LAYER_RELEASE, 0, disp->osd_layer, 0, 0);
err_video_layer:
err_version:
	close(disp->fd);
err_open:
	free(disp);
	return NULL;
}

static void sunxi_disp_close(struct sunxi_disp *sunxi_disp)
{
	struct sunxi_disp_private *disp = (struct sunxi_disp_private *)sunxi_disp;

	DISP(DISP_CMD_LAYER_CLOSE, 0, disp->video_layer, 0, 0);
	DISP(DISP_CMD_LAYER_RELEASE, 0, disp->video_layer, 0, 0);

	if (disp->osd_layer)
	{
		DISP(DISP_CMD_LAYER_CLOSE, 0, disp->osd_layer, 0, 0);
		DISP(DISP_CMD_LAYER_RELEASE, 0, disp->osd_layer, 0, 0);
	}

	close(disp->fd);
	free(sunxi_disp);
}

static void set_ycbcr_format(__disp_fb_t* fb, VdpYCbCrFormat format)
{
    switch (format) {
        case VDP_YCBCR_FORMAT_YUYV:
            fb->mode = DISP_MOD_INTERLEAVED;
            fb->format = DISP_FORMAT_YUV422;
            fb->seq = DISP_SEQ_YUYV;
            break;
        case VDP_YCBCR_FORMAT_UYVY:
            fb->mode = DISP_MOD_INTERLEAVED;
            fb->format = DISP_FORMAT_YUV422;
            fb->seq = DISP_SEQ_UYVY;
            break;
        case VDP_YCBCR_FORMAT_NV12:
            fb->mode = DISP_MOD_NON_MB_UV_COMBINED;
            fb->format = DISP_FORMAT_YUV420;
            fb->seq = DISP_SEQ_UVUV;
            break;
        case VDP_YCBCR_FORMAT_YV12:
            fb->mode = DISP_MOD_NON_MB_PLANAR;
            fb->format = DISP_FORMAT_YUV420;
            fb->seq = DISP_SEQ_UVUV;
            break;
        default:
        case INTERNAL_YCBCR_FORMAT:
            fb->mode = DISP_MOD_MB_UV_COMBINED;
            fb->format = DISP_FORMAT_YUV420;
            fb->seq = DISP_SEQ_UVUV;
            break;
    }
}

void set_video_info_geometry(
    __disp_layer_info_t* v, int x, int y, const output_surface_ctx_t* surface)
{
    v->fb.addr[0] = cedrus_mem_get_phys_addr(surface->yuv->data);
    v->fb.addr[1] = cedrus_mem_get_phys_addr(surface->yuv->data) + surface->vs->luma_size;
    v->fb.addr[2] = cedrus_mem_get_phys_addr(surface->yuv->data) + surface->vs->luma_size + surface->vs->chroma_size / 2;

    v->fb.size.width = surface->vs->width;
    v->fb.size.height = surface->vs->height;

    v->src_win.x = surface->video_src_rect.x0;
    v->src_win.y = surface->video_src_rect.y0;
    v->src_win.width = surface->video_src_rect.x1 - surface->video_src_rect.x0;
    v->src_win.height = surface->video_src_rect.y1 - surface->video_src_rect.y0;

    v->scn_win.x = x + surface->video_dst_rect.x0;
    v->scn_win.y = y + surface->video_dst_rect.y0;
    v->scn_win.width = surface->video_dst_rect.x1 - surface->video_dst_rect.x0;
    v->scn_win.height = surface->video_dst_rect.y1 - surface->video_dst_rect.y0;

    if (v->scn_win.y < 0 && v->scn_win.height != 0)
    {
        const int scn_clip = -(v->scn_win.y);
        const int src_clip = scn_clip * v->src_win.height / v->scn_win.height;
        v->src_win.y += src_clip;
        v->src_win.height -= src_clip;
        v->scn_win.y = 0;
        v->scn_win.height -= scn_clip;
    }

    // Log coords and sizes each time any of them changes.
    static char prevCoordsStr[1000] = {0};
    char coordsStr[1000];
    snprintf(coordsStr, sizeof(coordsStr),
        "video %d x %d @(%d, %d); surface %d x %d @(%d, %d); fb %d x %d",
        v->src_win.width, v->src_win.height, v->src_win.x, v->src_win.y,
        v->scn_win.width, v->scn_win.height, v->scn_win.x, v->scn_win.y,
        v->fb.size.width, v->fb.size.height);
    if (strcmp(coordsStr, prevCoordsStr) != 0)
    {
        strcpy(prevCoordsStr, coordsStr);
        VDPAU_DBG("[disp1] Coords: %s", coordsStr);
    }
}

static void set_enhancement_and_csc(
    struct sunxi_disp_private* disp, output_surface_ctx_t *surface, bool layerJustOpened)
{
    bool enhancementTurnedOff = false;
    // Note: might be more reliable (but slower and problematic when there
    // are driver issues and the GET functions return wrong values) to query the
    // old values instead of relying on our internal csc_change.
    // Since the driver calculates a matrix out of these values after each
    // set doing this unconditionally is costly.
    if (surface->csc_change)
    {
        if (!ini.disableCscMatrix)
        {
            DISP(DISP_CMD_LAYER_ENHANCE_OFF, 0, disp->video_layer, 0, 0);
            enhancementTurnedOff = true;
            if (ini.outputEnhancement)
                VDPAU_DBG("[disp1] ENHANCEMENT=OFF: needed before settings CSC Matrix");
            DISP(DISP_CMD_LAYER_SET_BRIGHT, 0, disp->video_layer, 0xff * surface->brightness + 0x20, 0);
            DISP(DISP_CMD_LAYER_SET_CONTRAST, 0, disp->video_layer, 0x20 * surface->contrast, 0);
            DISP(DISP_CMD_LAYER_SET_SATURATION, 0, disp->video_layer, 0x20 * surface->saturation, 0);
            // hue scale is randomly chosen, no idea how it maps exactly
            DISP(DISP_CMD_LAYER_SET_HUE, 0, disp->video_layer, (32 / 3.14) * surface->hue + 0x20, 0);
        }
        surface->csc_change = 0;
    }

    if ((layerJustOpened && ini.enhancementOn) || (!ini.enhancementOff && enhancementTurnedOff))
    {
        DISP(DISP_CMD_LAYER_ENHANCE_ON, 0, disp->video_layer, 0, 0);
        if (ini.outputEnhancement)
        {
            VDPAU_DBG("[disp1] ENHANCEMENT=ON: %s",
                ini.enhancementOn
                    ? ".ini enhancementOn=true"
                    : "restoring after setting CSC Matrix");
        }
    }
    else if (layerJustOpened && ini.enhancementOff && !enhancementTurnedOff)
    {
        DISP(DISP_CMD_LAYER_ENHANCE_OFF, 0, disp->video_layer, 0, 0);
        if (ini.outputEnhancement)
            VDPAU_DBG("[disp1] ENHANCEMENT=OFF: .ini enhancementOff=true");
    }
}

static int sunxi_disp_set_video_layer(struct sunxi_disp *sunxi_disp,
    int x, int y, int width, int height, output_surface_ctx_t *surface)
{
    /*unused*/ (void) width;
    /*unused*/ (void) height;

	struct sunxi_disp_private* disp = (struct sunxi_disp_private *) sunxi_disp;
	__disp_layer_info_t* const v = &disp->video_info;

    set_ycbcr_format(&v->fb, surface->vs->source_format);

    set_video_info_geometry(v, x, y, surface);

	if (ini.surfaceSleepUs > 0)
		usleep(ini.surfaceSleepUs);

	bool layerJustOpened = false;
	// TODO: Consider rewriting: store and compare prev video_info.
	if (!ini.enableSetFb || !disp->video_layer_opened || surface->csc_change)
	{
		DISP(DISP_CMD_LAYER_SET_PARA, 0, disp->video_layer, &disp->video_info, 0);
		DISP(DISP_CMD_LAYER_OPEN, 0, disp->video_layer, 0, 0);
		disp->video_layer_opened = true;
		layerJustOpened = true;

        if (ini.enableSetFb)
			VDPAU_DBG("[disp1] Video Layer opened: DISP_CMD_LAYER_SET_PARA, DISP_CMD_LAYER_OPEN");
	}
	else
	{
		DISP(DISP_CMD_LAYER_SET_FB, 0, disp->video_layer, &v->fb, 0);
	}

    set_enhancement_and_csc(disp, surface, layerJustOpened);

	return 0;
}

static void sunxi_disp_close_video_layer(struct sunxi_disp *sunxi_disp)
{
	struct sunxi_disp_private *disp = (struct sunxi_disp_private *)sunxi_disp;

	DISP(DISP_CMD_LAYER_CLOSE, 0, disp->video_layer, 0, 0);
	disp->video_layer_opened = false;
}

static int sunxi_disp_set_osd_layer(struct sunxi_disp *sunxi_disp, int x, int y, int width, int height, output_surface_ctx_t *surface)
{
	struct sunxi_disp_private *disp = (struct sunxi_disp_private *)sunxi_disp;

	switch (surface->rgba.format)
	{
	case VDP_RGBA_FORMAT_R8G8B8A8:
		disp->osd_info.fb.br_swap = 1;
		break;
	case VDP_RGBA_FORMAT_B8G8R8A8:
	default:
		disp->osd_info.fb.br_swap = 0;
		break;
	}

	disp->osd_info.fb.addr[0] = cedrus_mem_get_phys_addr(surface->rgba.data);
	disp->osd_info.fb.size.width = surface->rgba.width;
	disp->osd_info.fb.size.height = surface->rgba.height;
	disp->osd_info.src_win.x = surface->rgba.dirty.x0;
	disp->osd_info.src_win.y = surface->rgba.dirty.y0;
	disp->osd_info.src_win.width = surface->rgba.dirty.x1 - surface->rgba.dirty.x0;
	disp->osd_info.src_win.height = surface->rgba.dirty.y1 - surface->rgba.dirty.y0;
	disp->osd_info.scn_win.x = x + surface->rgba.dirty.x0;
	disp->osd_info.scn_win.y = y + surface->rgba.dirty.y0;
	disp->osd_info.scn_win.width = min_nz(width, surface->rgba.dirty.x1) - surface->rgba.dirty.x0;
	disp->osd_info.scn_win.height = min_nz(height, surface->rgba.dirty.y1) - surface->rgba.dirty.y0;

	DISP(DISP_CMD_LAYER_SET_PARA, 0, disp->osd_layer, &disp->osd_info, 0);
	DISP(DISP_CMD_LAYER_OPEN, 0, disp->osd_layer, 0, 0);

	return 0;
}

static void sunxi_disp_close_osd_layer(struct sunxi_disp *sunxi_disp)
{
	struct sunxi_disp_private *disp = (struct sunxi_disp_private *)sunxi_disp;

	DISP(DISP_CMD_LAYER_CLOSE, 0, disp->osd_layer, 0, 0);
}
