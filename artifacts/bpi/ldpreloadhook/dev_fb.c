#include "dev_fb.h"

#include <stdint.h>

#include <sys/ioctl.h>
#include <linux/fb.h>

static void dumpUint32Ptr(char* argpStr, size_t argpLen, void* argp)
{
    if (argp == NULL)
        snprintf(argpStr, argpLen, "NULL");
    else
        snprintf(argpStr, argpLen, "&(%u)", *((const uint32_t*) argp));
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpEnumFbBlank(char* argpStr, size_t argpLen, void* argp)
{
    switch ((int) argp)
    {
        case FB_BLANK_UNBLANK: return snprintf(argpStr, argpLen, "FB_BLANK_UNBLANK");
        case FB_BLANK_NORMAL: return snprintf(argpStr, argpLen, "FB_BLANK_NORMAL");
        case FB_BLANK_VSYNC_SUSPEND: return snprintf(argpStr, argpLen, "FB_BLANK_VSYNC_SUSPEND");
        case FB_BLANK_HSYNC_SUSPEND: return snprintf(argpStr, argpLen, "FB_BLANK_HSYNC_SUSPEND");
        case FB_BLANK_POWERDOWN: return snprintf(argpStr, argpLen, "FB_BLANK_POWERDOWN");
        default: return snprintf(argpStr, argpLen, "(int) %d", (int) argp);
    }
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpConstFbType(char* s, size_t len, int value)
{
    switch (value)
    {
        case FB_TYPE_PACKED_PIXELS: return snprintf(s, len, "FB_TYPE_PACKED_PIXELS");
        case FB_TYPE_PLANES: return snprintf(s, len, "FB_TYPE_PLANES");
        case FB_TYPE_INTERLEAVED_PLANES: return snprintf(s, len, "FB_TYPE_INTERLEAVED_PLANES");
        case FB_TYPE_TEXT: return snprintf(s, len, "FB_TYPE_TEXT");
        case FB_TYPE_VGA_PLANES: return snprintf(s, len, "FB_TYPE_VGA_PLANES");
        case FB_TYPE_FOURCC: return snprintf(s, len, "FB_TYPE_FOURCC");
        default: return snprintf(s, len, "(int) %d", value);
    }
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpConstFbVisual(char* s, size_t len, int value)
{
    switch (value)
    {
        case FB_VISUAL_MONO01: return snprintf(s, len, "FB_VISUAL_MONO01");
        case FB_VISUAL_MONO10: return snprintf(s, len, "FB_VISUAL_MONO10");
        case FB_VISUAL_TRUECOLOR: return snprintf(s, len, "FB_VISUAL_TRUECOLOR");
        case FB_VISUAL_PSEUDOCOLOR: return snprintf(s, len, "FB_VISUAL_PSEUDOCOLOR");
        case FB_VISUAL_DIRECTCOLOR: return snprintf(s, len, "FB_VISUAL_DIRECTCOLOR");
        case FB_VISUAL_STATIC_PSEUDOCOLOR: return snprintf(s, len, "FB_VISUAL_STATIC_PSEUDOCOLOR");
        case FB_VISUAL_FOURCC: return snprintf(s, len, "FB_VISUAL_FOURCC");
        default: return snprintf(s, len, "(int) %d", value);
    }
}

static void dumpStructPtr(char* argpStr, size_t argpLen, void* argp, const char* structStr)
{
    if (!conf.logStructs)
        return;

    if (argp == NULL)
        snprintf(argpStr, argpLen, "(struct %s*) NULL", structStr);
    else
        snprintf(argpStr, argpLen, "(struct %s*)", structStr);
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpStructFbBitfield(char *s, size_t len,
    const struct fb_bitfield* p, const char* prefix, const char* suffix)
{
    if (!conf.logStructs)
        return 0;
    return snprintf(s, len, "%s{ offset: %u, length: %u, msb_right: %u }%s",
        prefix, p->offset, p->length, p->msb_right, suffix);
}

/** @return Number of chars printed excluding '\0'. Reset flag in flags to 0. **/
static int dumpFlag(char *s, size_t len,
    bool* needSep, __u32* flags, __u32 flag, const char* flagName)
{
    int result = 0;
    if (*flags & flag)
    {
        result = snprintf(s, len, *needSep ? " | %s" : "%s", flagName);
        *needSep = true;
        *flags &= ~flag;
    }
    return result;
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpFlagsFbActivate(char *s, size_t len,
    __u32 value, const char* prefix, const char* suffix)
{
    if (value == 0)
        return snprintf(s, len, "%sFB_ACTIVATE_NOW%s", prefix, suffix);
    __u32 flags = value;
    char* ss = s;
    bool needSep = false;
    ss += snprintf(ss, len - (ss - s), "%s", prefix);
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_ACTIVATE_NXTOPEN, "FB_ACTIVATE_NXTOPEN");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_ACTIVATE_TEST, "FB_ACTIVATE_TEST");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_ACTIVATE_VBL, "FB_ACTIVATE_VBL");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_CHANGE_CMAP_VBL, "FB_CHANGE_CMAP_VBL");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_ACTIVATE_ALL, "FB_ACTIVATE_ALL");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_ACTIVATE_FORCE, "FB_ACTIVATE_FORCE");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_ACTIVATE_INV_MODE, "FB_ACTIVATE_INV_MODE");
    if (flags != 0) //< Print remaining flags as hex.
        ss += snprintf(ss, len - (ss - s), needSep ? " | 0x%X" : "0x%X", flags);
    ss += snprintf(ss, len - (ss - s), "%s", suffix);
    return (int) (ss - s);
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpFlagsFbSync(char *s, size_t len,
    __u32 value, const char* prefix, const char* suffix)
{
    if (value == 0)
        return snprintf(s, len, "%s0%s", prefix, suffix);
    __u32 flags = value;
    char* ss = s;
    bool needSep = false;
    ss += snprintf(ss, len - (ss - s), "%s", prefix);
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_SYNC_HOR_HIGH_ACT, "FB_SYNC_HOR_HIGH_ACT");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_SYNC_VERT_HIGH_ACT, "FB_SYNC_VERT_HIGH_ACT");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_SYNC_EXT, "FB_SYNC_EXT");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_SYNC_COMP_HIGH_ACT, "FB_SYNC_COMP_HIGH_ACT");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_SYNC_BROADCAST, "FB_SYNC_BROADCAST");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_SYNC_ON_GREEN, "FB_SYNC_ON_GREEN");
    if (flags != 0) //< Print remaining flags as hex.
        ss += snprintf(ss, len - (ss - s), needSep ? " | 0x%X" : "0x%X", flags);
    ss += snprintf(ss, len - (ss - s), "%s", suffix);
    return (int) (ss - s);
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpFlagsFbVmode(char *s, size_t len,
    __u32 value, const char* prefix, const char* suffix)
{
    if (value == 0)
        return snprintf(s, len, "%sFB_VMODE_NONINTERLACED%s", prefix, suffix);
    __u32 flags = value;
    char* ss = s;
    bool needSep = false;
    ss += snprintf(ss, len - (ss - s), "%s", prefix);
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VMODE_INTERLACED, "FB_VMODE_INTERLACED");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VMODE_DOUBLE, "FB_VMODE_DOUBLE");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VMODE_ODD_FLD_FIRST, "FB_VMODE_ODD_FLD_FIRST");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VMODE_YWRAP, "FB_VMODE_YWRAP");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VMODE_SMOOTH_XPAN, "FB_VMODE_SMOOTH_XPAN|FB_VMODE_CONUPDATE");
    if (flags != 0) //< Print remaining flags as hex.
        ss += snprintf(ss, len - (ss - s), needSep ? " | 0x%X" : "0x%X", flags);
    ss += snprintf(ss, len - (ss - s), "%s", suffix);
    return (int) (ss - s);
}

/** @return Number of chars printed excluding '\0'. **/
static int dumpFlagsFbVblank(char *s, size_t len,
    __u32 value, const char* prefix, const char* suffix)
{
    if (value == 0)
        return snprintf(s, len, "%s0%s", prefix, suffix);
    __u32 flags = value;
    char* ss = s;
    bool needSep = false;
    ss += snprintf(ss, len - (ss - s), "%s", prefix);
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_VBLANKING, "FB_VBLANK_VBLANKING");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_HBLANKING, "FB_VBLANK_HBLANKING");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_HAVE_VBLANK, "FB_VBLANK_HAVE_VBLANK");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_HAVE_HBLANK, "FB_VBLANK_HAVE_HBLANK");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_HAVE_COUNT, "FB_VBLANK_HAVE_COUNT");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_HAVE_VCOUNT, "FB_VBLANK_HAVE_VCOUNT");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_HAVE_HCOUNT, "FB_VBLANK_HAVE_HCOUNT");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_VSYNCING, "FB_VBLANK_VSYNCING");
    ss += dumpFlag(ss, len - (ss - s), &needSep, &flags, FB_VBLANK_HAVE_VSYNC, "FB_VBLANK_HAVE_VSYNC");
    if (flags != 0) //< Print remaining flags as hex.
        ss += snprintf(ss, len - (ss - s), needSep ? " | 0x%X" : "0x%X", flags);
    ss += snprintf(ss, len - (ss - s), "%s", suffix);
    return (int) (ss - s);
}

static void dumpStructFbVarScreenInfo(char* dataStr, size_t dataLen, void* argp)
{
    if (!conf.logStructs)
        return;
    if (argp == NULL)
        return;
    const struct fb_var_screeninfo* p = argp;
    char* s = dataStr;
    s += snprintf(s, dataLen - (s - dataStr),
        "\n{ xres: %u, yres: %u, xres_virtual: %u, yres_virtual: %u, xoffset: %u, yoffset: %u,\n",
        p->xres, p->yres, p->xres_virtual, p->yres_virtual, p->xoffset, p->yoffset);
    s += snprintf(s, dataLen - (s - dataStr),
        "  bits_per_pixel: %u, grayscale: %u,\n",
        p->bits_per_pixel, p->grayscale);
    s += dumpStructFbBitfield(s, dataLen - (s - dataStr), &p->red, "  red: ", ",\n");
    s += dumpStructFbBitfield(s, dataLen - (s - dataStr), &p->green, "  green: ", ",\n");
    s += dumpStructFbBitfield(s, dataLen - (s - dataStr), &p->blue, "  blue: ", ",\n");
    s += dumpStructFbBitfield(s, dataLen - (s - dataStr), &p->transp, "  transp: ", ",\n");
    s += snprintf(s, dataLen - (s - dataStr), "  nonstd: %u, ", p->nonstd);
    s += dumpFlagsFbActivate(s, dataLen - (s - dataStr), p->activate, "activate: ", ", ");
    s += snprintf(s, dataLen - (s - dataStr), "width: %u, height: %u, accel_flags: 0x%X,\n",
        p->width, p->height, p->accel_flags);
    s += snprintf(s, dataLen - (s - dataStr),
        "  pixclock: %u, left_margin: %u, right_margin: %u, upper_margin: %u, lower_margin: %u,\n",
        p->pixclock, p->left_margin, p->right_margin, p->upper_margin, p->lower_margin);
    s += snprintf(s, dataLen - (s - dataStr),
        "  hsync_len: %u, vsync_len: %u,\n",
        p->hsync_len, p->vsync_len);
    s += dumpFlagsFbSync(s, dataLen - (s - dataStr), p->sync, "  sync: ", ", ");
    s += dumpFlagsFbVmode(s, dataLen - (s - dataStr), p->sync, "vmode: ", ",\n");
    s += snprintf(s, dataLen - (s - dataStr),
        "  rotate: %u, colorspace: %u, reserved: {%u, %u, %u, %u}\n",
        p->rotate, p->colorspace, p->reserved[0], p->reserved[1], p->reserved[2], p->reserved[3]);
    snprintf(s, dataLen - (s - dataStr), "}");
}

static void dumpStructFbFixScreenInfo(char* dataStr, size_t dataLen, void* argp)
{
    if (!conf.logStructs)
        return;
    if (argp == NULL)
        return;
    const struct fb_fix_screeninfo* p = argp;
    char* s = dataStr;
    s += snprintf(s, dataLen - (s - dataStr), "\n{ id: \"");
    int i = 0;
    while (i < (int) sizeof(p->id) && p->id[i] != '\0' && s < dataStr + dataLen - 1)
        *(s++) = p->id[i++];
    s += snprintf(s, dataLen - (s - dataStr), "\", smem_start: %lu, smem_len: %u,\n",
        p->smem_start, p->smem_len);
    s += snprintf(s, dataLen - (s - dataStr), "  type: ");
    s += dumpConstFbType(s, dataLen - (s - dataStr), p->type);
    s += snprintf(s, dataLen - (s - dataStr), ", type_aux: %u,\n", p->type_aux);
    s += snprintf(s, dataLen - (s - dataStr), "  visual: ");
    s += dumpConstFbVisual(s, dataLen - (s - dataStr), p->type);
    s += snprintf(s, dataLen - (s - dataStr), ", xpanstep: %u, ypanstep: %u, ywrapstep: %u,\n",
        p->xpanstep, p->ypanstep, p->ywrapstep);
    s += snprintf(s, dataLen - (s - dataStr),
        "  line_length: %u, mmio_start: %lu, mmio_len: %u, accel: %u, capabilities: %u, reserved: {%u, %u}\n",
        p->line_length, p->mmio_start, p->mmio_len, p->accel, p->capabilities, p->reserved[0], p->reserved[1]);
    snprintf(s, dataLen - (s - dataStr), "}");
}

static void dumpStructFbCursor(char* dataStr, size_t dataLen, void* argp)
{
    if (!conf.logStructs)
        return;
    if (argp == NULL)
        return;
    const struct fb_cursor* p = argp;
    char* s = dataStr;
    s += snprintf(s, dataLen - (s - dataStr),
        "\n{ set: %u, enable: %u, rop: %u, mask: %s, hot: {x: %u, y: %u},\n",
        p->set, p->enable, p->rop, p->mask ? "ptr" : "NULL", p->hot.x, p->hot.y);
    const struct fb_image* d = &p->image;
    s += snprintf(s, dataLen - (s - dataStr),
        "  image: { dx: %u, dy: %u, width: %u, height: %u, fg_color: 0x%08X, bg_color: 0x%08X, depth: %u, data: %s, cmap }\n",
        d->dx, d->dy, d->width, d->height, d->fg_color, d->bg_color, d->depth, d->data ? "ptr" : "NULL");
    snprintf(s, dataLen - (s - dataStr), "}");
}

static void dumpStructFbVblank(char* dataStr, size_t dataLen, void* argp)
{
    if (!conf.logStructs)
        return;
    if (argp == NULL)
        return;
    const struct fb_vblank* p = argp;
    char* s = dataStr;
    s += dumpFlagsFbVblank(s, dataLen - (s - dataStr), p->flags, "  { flags: ", ",\n");
    s += snprintf(s, dataLen - (s - dataStr),
        "  count: %u, vcount: %u, hcount: %u, reserved: {%u, %u, %u, %u}\n",
        p->count, p->vcount, p->hcount, p->reserved[0], p->reserved[1], p->reserved[2], p->reserved[3]);
    snprintf(s, dataLen - (s - dataStr), "}");
}

static void devFbRequestAndArgpToStr(request_t request, void* argp,
    char* requestStr, size_t requestLen, char* argpStr, size_t argpLen, char* dataStr, size_t dataLen)
{
    requestStr[0] = '\0';
    argpStr[0] = '\0';
    dataStr[0] = '\0';

    switch ((int) request)
    {
        case FBIOGET_VSCREENINFO:
            snprintf(requestStr, requestLen, "FBIOGET_VSCREENINFO");
            dumpStructPtr(argpStr, argpLen, argp, "fb_var_screeninfo");
            dumpStructFbVarScreenInfo(dataStr, dataLen, argp);
            break;
        case FBIOPUT_VSCREENINFO:
            snprintf(requestStr, requestLen, "FBIOPUT_VSCREENINFO");
            dumpStructPtr(argpStr, argpLen, argp, "fb_var_screeninfo");
            dumpStructFbVarScreenInfo(dataStr, dataLen, argp);
            break;
        case FBIOGET_FSCREENINFO:
            snprintf(requestStr, requestLen, "FBIOGET_FSCREENINFO");
            dumpStructPtr(argpStr, argpLen, argp, "fb_fix_screeninfo");
            dumpStructFbFixScreenInfo(dataStr, dataLen, argp);
            break;
        case FBIOGETCMAP:
            snprintf(requestStr, requestLen, "FBIOGETCMAP");
            break;
        case FBIOPUTCMAP:
            snprintf(requestStr, requestLen, "FBIOPUTCMAP");
            break;
        case FBIOPAN_DISPLAY:
            snprintf(requestStr, requestLen, "FBIOPAN_DISPLAY");
            dumpStructPtr(argpStr, argpLen, argp, "fb_var_screeninfo");
            dumpStructFbVarScreenInfo(dataStr, dataLen, argp);
            break;
        case FBIO_CURSOR:
            snprintf(requestStr, requestLen, "FBIO_CURSOR");
            dumpStructPtr(argpStr, argpLen, argp, "fb_cursor");
            dumpStructFbCursor(dataStr, dataLen, argp);
            break;
        case /*FBIOGET_MONITORSPEC*/ 0x460C:
            snprintf(requestStr, requestLen, "FBIOGET_MONITORSPEC");
            break;
        case /*FBIOPUT_MONITORSPEC*/0x460D:
            snprintf(requestStr, requestLen, "FBIOPUT_MONITORSPEC");
            break;
        case /*FBIOSWITCH_MONIBIT*/ 0x460E:
            snprintf(requestStr, requestLen, "FBIOSWITCH_MONIBIT");
            break;
        case FBIOGET_CON2FBMAP:
            snprintf(requestStr, requestLen, "FBIOGET_CON2FBMAP");
            break;
        case FBIOPUT_CON2FBMAP:
            snprintf(requestStr, requestLen, "FBIOPUT_CON2FBMAP");
            break;
        case FBIOBLANK:
            snprintf(requestStr, requestLen, "FBIOBLANK");
            dumpEnumFbBlank(argpStr, argpLen, argp);
            break;
        case FBIOGET_VBLANK:
            snprintf(requestStr, requestLen, "FBIOGET_VBLANK");
            dumpStructPtr(argpStr, argpLen, argp, "fb_vblank");
            dumpStructFbVblank(dataStr, dataLen, argp);
            break;
        case FBIO_ALLOC:
            snprintf(requestStr, requestLen, "FBIO_ALLOC");
            break;
        case FBIO_FREE:
            snprintf(requestStr, requestLen, "FBIO_FREE");
            break;
        case FBIOGET_GLYPH:
            snprintf(requestStr, requestLen, "FBIOGET_GLYPH");
            break;
        case FBIOGET_HWCINFO:
            snprintf(requestStr, requestLen, "FBIOGET_HWCINFO");
            break;
        case FBIOPUT_MODEINFO:
            snprintf(requestStr, requestLen, "FBIOPUT_MODEINFO");
            break;
        case FBIOGET_DISPINFO:
            snprintf(requestStr, requestLen, "FBIOGET_DISPINFO");
            break;
        case FBIO_WAITFORVSYNC:
            snprintf(requestStr, requestLen, "FBIO_WAITFORVSYNC");
            dumpUint32Ptr(argpStr, argpLen, argp);
        default:
        {
        }
    }

    // Fill strings with default representation if not filled above.
    if (requestStr[0] == '\0')
        snprintf(requestStr, requestLen, "request: 0x%08X", (uint32_t) request);
    if (argpStr[0] == '\0')
        snprintf(argpStr, argpLen, "argp: 0x%08X", (uint32_t) argp);
}

void hookPrintIoctlDevFb(const char* prefix,
    const char* filename, int fd, request_t request, void* argp, int res)
{
    /*potentially unused*/ (void) filename;
    /*potentially unused*/ (void) fd;
    /*potentially unused*/ (void) res;

    char requestStr[100];
    char argpStr[100];
    char dataStr[1000];

    devFbRequestAndArgpToStr(request, argp,
        requestStr, sizeof(requestStr), argpStr, sizeof(argpStr), dataStr, sizeof(dataStr));

    PRINT("%s: ioctl(fd: %d \"%s\", %s, %s) -> %d%s", prefix,
        fd, filename, requestStr, argpStr, res, dataStr);
}
