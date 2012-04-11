// dxva.cpp : Defines the entry point for the console application.
//

#ifdef Q_OS_WIN
#include "dxva.h"
#include "base/log.h"

const UINT BACK_BUFFER_COUNT = 0;

const D3DFORMAT VIDEO_RENDER_TARGET_FORMAT = D3DFMT_X8R8G8B8;

static const GUID DXVA2_ModeMPEG2_MoComp = {
    0xe6a9f44b, 0x61b0,0x4563, {0x9e,0xa4,0x63,0xd2,0xa3,0xc6,0xfe,0x66}
};
static const GUID DXVA2_ModeMPEG2_IDCT = {
    0xbf22ad00, 0x03ea,0x4690, {0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e}
};
static const GUID DXVA2_ModeMPEG2_VLD = {
    0xee27417f, 0x5e28,0x4e65, {0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9}
};

static const GUID DXVA2_ModeH264_A = {
    0x1b81be64, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_B = {
    0x1b81be65, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_C = {
    0x1b81be66, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_D = {
    0x1b81be67, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_E = {
    0x1b81be68, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeH264_F = {
    0x1b81be69, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVADDI_Intel_ModeH264_A = {
    0x604F8E64, 0x4951,0x4c54, {0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6}
};
static const GUID DXVADDI_Intel_ModeH264_C = {
    0x604F8E66, 0x4951,0x4c54, {0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6}
};
static const GUID DXVADDI_Intel_ModeH264_E = {
    0x604F8E68, 0x4951,0x4c54, {0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6}
};
static const GUID DXVA2_ModeWMV8_A = {
    0x1b81be80, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV8_B = {
    0x1b81be81, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV9_A = {
    0x1b81be90, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV9_B = {
    0x1b81be91, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeWMV9_C = {
    0x1b81be94, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};

static const GUID DXVA2_ModeVC1_A = {
    0x1b81beA0, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeVC1_B = {
    0x1b81beA1, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeVC1_C = {
    0x1b81beA2, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};
static const GUID DXVA2_ModeVC1_D = {
    0x1b81beA3, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};

static const GUID DXVA_NoEncrypt = {
    0x1b81bed0, 0xa0c7,0x11d3, {0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5}
};

struct dxva2_mode_t
{
    const char   *name;
    const GUID   *guid;
    int          codec;
};

/* XXX Prefered modes must come first */
static const dxva2_mode_t dxva2_modes[] = {
    { "MPEG-2 variable-length decoder",            &DXVA2_ModeMPEG2_VLD,     CODEC_ID_MPEG2VIDEO },
    { "MPEG-2 motion compensation",                &DXVA2_ModeMPEG2_MoComp,  0 },
    { "MPEG-2 inverse discrete cosine transform",  &DXVA2_ModeMPEG2_IDCT,    0 },

    { "H.264 variable-length decoder, film grain technology",                      &DXVA2_ModeH264_F,         CODEC_ID_H264 },
    { "H.264 variable-length decoder, no film grain technology",                   &DXVA2_ModeH264_E,         CODEC_ID_H264 },
    { "H.264 variable-length decoder, no film grain technology (Intel)",           &DXVADDI_Intel_ModeH264_E, CODEC_ID_H264 },
    { "H.264 inverse discrete cosine transform, film grain technology",            &DXVA2_ModeH264_D,         0             },
    { "H.264 inverse discrete cosine transform, no film grain technology",         &DXVA2_ModeH264_C,         0             },
    { "H.264 inverse discrete cosine transform, no film grain technology (Intel)", &DXVADDI_Intel_ModeH264_C, 0             },
    { "H.264 motion compensation, film grain technology",                          &DXVA2_ModeH264_B,         0             },
    { "H.264 motion compensation, no film grain technology",                       &DXVA2_ModeH264_A,         0             },
    { "H.264 motion compensation, no film grain technology (Intel)",               &DXVADDI_Intel_ModeH264_A, 0             },

    { "Windows Media Video 8 motion compensation", &DXVA2_ModeWMV8_B, 0 },
    { "Windows Media Video 8 post processing",     &DXVA2_ModeWMV8_A, 0 },

    { "Windows Media Video 9 IDCT",                &DXVA2_ModeWMV9_C, 0 },
    { "Windows Media Video 9 motion compensation", &DXVA2_ModeWMV9_B, 0 },
    { "Windows Media Video 9 post processing",     &DXVA2_ModeWMV9_A, 0 },

    { "VC-1 variable-length decoder",              &DXVA2_ModeVC1_D, CODEC_ID_VC1 },
    { "VC-1 variable-length decoder",              &DXVA2_ModeVC1_D, CODEC_ID_WMV3 },
    { "VC-1 inverse discrete cosine transform",    &DXVA2_ModeVC1_C, 0 },
    { "VC-1 motion compensation",                  &DXVA2_ModeVC1_B, 0 },
    { "VC-1 post processing",                      &DXVA2_ModeVC1_A, 0 },

    { NULL, NULL, 0 }
};

static const dxva2_mode_t *Dxva2FindMode(const GUID& guid)
{
    for (unsigned i = 0; dxva2_modes[i].name; i++) {
        if (IsEqualGUID(*(dxva2_modes[i].guid), guid))
            return &dxva2_modes[i];
    }
    return NULL;
}

/* */
struct d3d_format_t
{
    const char   *name;
    D3DFORMAT    format;
//    FOURCC codec;
};

/* XXX Prefered format must come first */
static const d3d_format_t d3d_formats[] = {
    { "YV12",   (D3DFORMAT)MAKEFOURCC('Y','V','1','2')}, // YUV420
    { "NV12",   (D3DFORMAT)MAKEFOURCC('N','V','1','2')}, // Planar Y Packet UV (420)

    { NULL, (D3DFORMAT)0 }
};

static const d3d_format_t *D3dFindFormat(D3DFORMAT format)
{
    for (unsigned i = 0; d3d_formats[i].name; i++) {
        if (d3d_formats[i].format == format)
            return &d3d_formats[i];
    }
    return NULL;
}

DxvaSupportObject::DxvaSupportObject(int codecId)
:
    m_codecId(codecId),
    m_d3dobj(0),
    m_d3ddev(0),
    m_token(0),
    m_devmng(0),
    m_device(0),
    m_vs(0),
    m_render((D3DFORMAT)0),
    m_decoder(0),
    m_output((D3DFORMAT)0),
    m_surfaceCount(0),
    m_surfaceOrder(0),
    m_surfaceWidth(0),
    m_surfaceHeight(0),
    surface_chroma(0)
{
}

bool DxvaSupportObject::newDxva()
{
    /* */
    if (!D3dCreateDevice()) {
        cl_log.log(cl_logERROR, "Failed to create Direct3D device");
        return false;
    }
    cl_log.log(cl_logDEBUG1, "D3dCreateDevice succeed");

    if (!D3dCreateDeviceManager()) {
        cl_log.log(cl_logERROR, "D3dCreateDeviceManager failed");
        return false;
    }

    if (!DxCreateVideoService()) {
        cl_log.log(cl_logERROR, "DxCreateVideoService failed");
        return false;
    }

    /* */
    if (!DxFindVideoServiceConversion(&m_input, &m_render)) {
        cl_log.log(cl_logERROR, "DxFindVideoServiceConversion failed");
        return false;
    }

    m_description = DxDescribe();
    return true;
}

void DxvaSupportObject::initHw()
{
    m_hw.decoder = m_decoder;
    m_hw.cfg = &m_cfg;
    m_hw.surface_count = m_surfaceCount;
    m_hw.surface = m_hwSurface;
    for (unsigned i = 0; i < m_surfaceCount; i++)
        m_hw.surface[i] = m_surface[i].d3d;
}

void DxvaSupportObject::close()
{
    DxDestroyVideoConversion();
    DxDestroyVideoDecoder();
    DxDestroyVideoService();
    D3dDestroyDeviceManager();
    D3dDestroyDevice();
}

bool DxvaSupportObject::testDevice()
{
    /* Check the device */
    HRESULT hr = m_devmng->TestDevice(m_device);
    if (hr == DXVA2_E_NEW_VIDEO_DEVICE) {
        if (!DxResetVideoDecoder())
            return false;
    } else if (FAILED(hr)) {
        cl_log.log(cl_logERROR, "IDirect3DDeviceManager9_TestDevice %u", (unsigned)hr);
        return false;
    }

    return true;
}

/**
* It destroys a DirectX video service
*/
void DxvaSupportObject::DxDestroyVideoService()
{
    if (m_device)
        m_devmng->CloseDeviceHandle(m_device);

    if (m_vs)
        m_vs->Release();
}

/**
* It releases a Direct3D device and its resources.
*/
void DxvaSupportObject::D3dDestroyDevice()
{
    if (m_d3ddev)
        m_d3ddev->Release();

    if (m_d3dobj)
        m_d3dobj->Release();
}

bool DxvaSupportObject::D3dCreateDevice()
{
	HRESULT hr;

	D3dDestroyDevice();

	m_d3dobj = Direct3DCreate9(D3D_SDK_VERSION);

	if (m_d3dobj == NULL)
	{
		cl_log.log(cl_logERROR, "Direct3DCreate9 failed.\n");
		return false;
	}

	if (FAILED(m_d3dobj->GetAdapterIdentifier(
		D3DADAPTER_DEFAULT, 0, &m_d3dai))) {
			cl_log.log(cl_logERROR, "IDirect3D9_GetAdapterIdentifier failed");
			ZeroMemory(&m_d3dai, sizeof(m_d3dai));
	}

	ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));

	m_d3dpp.Flags                      = D3DPRESENTFLAG_VIDEO;
	m_d3dpp.Windowed                   = TRUE;
	m_d3dpp.hDeviceWindow              = NULL;
	m_d3dpp.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
	m_d3dpp.MultiSampleType            = D3DMULTISAMPLE_NONE;
	m_d3dpp.PresentationInterval       = D3DPRESENT_INTERVAL_DEFAULT; // D3DPRESENT_INTERVAL_ONE;
	m_d3dpp.BackBufferCount            = BACK_BUFFER_COUNT;
	m_d3dpp.BackBufferFormat           = VIDEO_RENDER_TARGET_FORMAT;
	m_d3dpp.BackBufferWidth  = 0;
	m_d3dpp.BackBufferHeight = 0;
	m_d3dpp.EnableAutoDepthStencil = FALSE;

	m_d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

	// First try to create a hardware D3D9 device.
	hr = m_d3dobj->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		GetShellWindow(),
		D3DCREATE_SOFTWARE_VERTEXPROCESSING |
		// D3DCREATE_FPU_PRESERVE |
		D3DCREATE_MULTITHREADED
		,
		&m_d3dpp,
		&m_d3ddev
		);

	if (FAILED(hr))
	{
		cl_log.log(cl_logERROR, "CreateDevice(HAL) failed with error 0x%x.\n", hr);
	}

	if (!m_d3ddev)
	{
		return false;
	}
	return true;
}

//-------------------------------------------------------------------
// D3dCreateDeviceManager
//
// Create the DXVA2 video processor device, and initialize
// DXVA2 resources.
//-------------------------------------------------------------------

bool DxvaSupportObject::D3dCreateDeviceManager()
{
    if (FAILED(DXVA2CreateDirect3DDeviceManager9(&m_token, &m_devmng))) {
        cl_log.log(cl_logERROR, "DXVA2CreateDirect3DDeviceManager9 failed");
        return false;
    }

    HRESULT hr = m_devmng->ResetDevice(m_d3ddev, m_token);
    if (FAILED(hr)) {
        cl_log.log(cl_logERROR, "IDirect3DDeviceManager9_ResetDevice failed: %08x", (unsigned)hr);
        return false;
    }

    return true;
}

bool DxvaSupportObject::DxCreateVideoService()
{
    HRESULT hr;

    hr = m_devmng->OpenDeviceHandle(&m_device);
    if (FAILED(hr)) {
        cl_log.log(cl_logERROR, "OpenDeviceHandle failed");
        return false;
    }

    hr = m_devmng->GetVideoService(m_device,IID_PPV_ARGS(&m_vs));
    if (FAILED(hr)) {
        cl_log.log(cl_logERROR, "GetVideoService failed");
        return false;
    }

    return true;
}

bool DxvaSupportObject::DxFindVideoServiceConversion(GUID *input, D3DFORMAT *output)
{
    UINT input_count = 0;
    GUID *input_list = NULL;
    if (FAILED(m_vs->GetDecoderDeviceGuids(&input_count, &input_list))) {
            cl_log.log(cl_logERROR, "IDirectXVideoDecoderService_GetDecoderDeviceGuids failed");
            return false;
    }

    for (unsigned i = 0; i < input_count; i++) {
        const GUID *g = &input_list[i];
        const dxva2_mode_t *mode = Dxva2FindMode(*g);
        if (mode) {
            cl_log.log(cl_logDEBUG1, "- '%s' is supported by hardware", mode->name);
        } else {
            cl_log.log(cl_logWARNING, "- Unknown GUID = %08X-%04x-%04x-XXXX",
                (unsigned)g->Data1, g->Data2, g->Data3);
        }
    }

    /* Try all supported mode by our priority */
    for (unsigned i = 0; dxva2_modes[i].name; i++) {
        const dxva2_mode_t *mode = &dxva2_modes[i];
        if (!mode->codec || mode->codec != m_codecId)
            continue;

        /* */
        BOOL is_suported = FALSE;
        for (const GUID *g = &input_list[0]; !is_suported && g < &input_list[input_count]; g++) {
            is_suported = IsEqualGUID(*mode->guid, *g);
        }
        if (!is_suported)
            continue;

        /* */
        cl_log.log(cl_logDEBUG1, "Trying to use '%s' as input", mode->name);
        UINT      output_count = 0;
        D3DFORMAT *output_list = NULL;
        if (FAILED(m_vs->GetDecoderRenderTargets(*mode->guid,
            &output_count,
            &output_list))) {
                cl_log.log(cl_logERROR, "IDirectXVideoDecoderService_GetDecoderRenderTargets failed");
                continue;
        }
        for (unsigned j = 0; j < output_count; j++) {
            const D3DFORMAT f = output_list[j];
            const d3d_format_t *format = D3dFindFormat(f);
            if (format) {
                cl_log.log(cl_logINFO, "%s is supported for output", format->name);
            } else {
                cl_log.log(cl_logINFO, "%s is supported for output (%4.4s)", f, (const char*)&f);
            }
        }

        /* */
        for (unsigned j = 0; d3d_formats[j].name; j++) {
            const d3d_format_t *format = &d3d_formats[j];

            /* */
            bool is_suported = false;
            for (unsigned k = 0; !is_suported && k < output_count; k++) {
                is_suported = format->format == output_list[k];
            }
            if (!is_suported)
                continue;

            /* We have our solution */
            cl_log.log(cl_logINFO, "Using '%s' to decode to '%s'", mode->name, format->name);
            *input  = *mode->guid;
            *output = format->format;
            CoTaskMemFree(output_list);
            CoTaskMemFree(input_list);
            return true;
        }
        CoTaskMemFree(output_list);
    }
    CoTaskMemFree(input_list);
    return false;
}

QString DxvaSupportObject::DxDescribe()
{
    static const struct {
        unsigned id;
        char     name[32];
    } vendors [] = {
        { 0x1002, "ATI" },
        { 0x10DE, "NVIDIA" },
        { 0x8086, "Intel" },
        { 0x5333, "S3 Graphics" },
        { 0, 0 }
    };
    const char *vendor = "Unknown";
    for (int i = 0; vendors[i].id != 0; i++) {
        if (vendors[i].id == m_d3dai.VendorId) {
            vendor = vendors[i].name;
            break;
        }
    }

    QString desc;
    desc = desc.sprintf("DXVA2 (%s, vendor %d(%s), device %d, revision %d)", m_d3dai.Description, m_d3dai.VendorId, vendor, m_d3dai.DeviceId, m_d3dai.Revision);
    return desc;
}

void DxvaSupportObject::DxDestroyVideoConversion()
{
    CopyCleanCache(&m_surfaceCache);
}

void DxvaSupportObject::DxDestroyVideoDecoder()
{
    if (m_decoder)
        m_decoder->Release();

    m_decoder = NULL;

    for (unsigned i = 0; i < m_surfaceCount; i++)
        m_surface[i].d3d->Release();

    m_surfaceCount = 0;
}

bool DxvaSupportObject::DxCreateVideoDecoder(int codec_id, int width, int height)
{
    /* */
    cl_log.log(cl_logINFO, "DxCreateVideoDecoder id %d %dx%d", codec_id, width, height);

    this->m_width  = width;
    this->m_height = height;

    /* Allocates all surfaces needed for the decoder */
    m_surfaceWidth  = (width  + 15) & ~15;
    m_surfaceHeight = (height + 15) & ~15;
    switch (codec_id) {
    case CODEC_ID_H264:
        m_surfaceCount = 16 + 1;
        break;
    default:
        m_surfaceCount = 2 + 1;
        break;
    }
    LPDIRECT3DSURFACE9 surface_list[VA_DXVA2_MAX_SURFACE_COUNT];
    if (FAILED(m_vs->CreateSurface(
        m_surfaceWidth,
        m_surfaceHeight,
        m_surfaceCount - 1,
        m_render,
        D3DPOOL_DEFAULT,
        0,
        DXVA2_VideoDecoderRenderTarget,
        surface_list,
        NULL))) {
            cl_log.log(cl_logERROR, "IDirectXVideoAccelerationService_CreateSurface failed\n");
            m_surfaceCount = 0;
            return false;
    }
    for (unsigned i = 0; i < m_surfaceCount; i++) {
        DxvaSurface *surface = &m_surface[i];
        surface->d3d = surface_list[i];
        surface->refcount = 0;
        surface->order = 0;
    }

    cl_log.log(cl_logINFO, "IDirectXVideoAccelerationService_CreateSurface succeed with %d surfaces (%dx%d)\n", m_surfaceCount, width, height);

    /* */
    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = width;
    dsc.SampleHeight    = height;
    dsc.Format          = m_render;
    //if (fmt->i_frame_rate > 0 && fmt->i_frame_rate_base > 0) {
    //    dsc.InputSampleFreq.Numerator   = fmt->i_frame_rate;
    //    dsc.InputSampleFreq.Denominator = fmt->i_frame_rate_base;
    //} else {
        dsc.InputSampleFreq.Numerator   = 0;
        dsc.InputSampleFreq.Denominator = 0;
//    }
    dsc.OutputFrameFreq = dsc.InputSampleFreq;
    dsc.UABProtectionLevel = FALSE;
    dsc.Reserved = 0;

#undef SampleFormat
    /* FIXME I am unsure we can let unknown everywhere */
    DXVA2_ExtendedFormat *ext = &dsc.SampleFormat;
    ext->SampleFormat = 0;//DXVA2_SampleUnknown;
    ext->VideoChromaSubsampling = 0;//DXVA2_VideoChromaSubsamplinva->Unknown;
    ext->NominalRange = 0;//DXVA2_NominalRange_Unknown;
    ext->VideoTransferMatrix = 0;//DXVA2_VideoTransferMatrix_Unknown;
    ext->VideoLighting = 0;//DXVA2_VideoLightinva->Unknown;
    ext->VideoPrimaries = 0;//DXVA2_VideoPrimaries_Unknown;
    ext->VideoTransferFunction = 0;//DXVA2_VideoTransFunc_Unknown;

    /* List all configurations available for the decoder */
    UINT                      cfg_count = 0;
    DXVA2_ConfigPictureDecode *cfg_list = NULL;
    if (FAILED(m_vs->GetDecoderConfigurations(
        m_input,
        &dsc,
        NULL,
        &cfg_count,
        &cfg_list))) {
            cl_log.log(cl_logERROR, "IDirectXVideoDecoderService_GetDecoderConfigurations failed\n");
            return false;
    }
    cl_log.log(cl_logINFO, "we got %d decoder configurations", cfg_count);

    /* Select the best decoder configuration */
    int cfg_score = 0;
    for (unsigned i = 0; i < cfg_count; i++) {
        const DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

        /* */
        cl_log.log(cl_logINFO, "configuration[%d] ConfigBitstreamRaw %d", i, cfg->ConfigBitstreamRaw);

        /* */
        int score;
        if (cfg->ConfigBitstreamRaw == 1)
            score = 1;
        else if (codec_id == CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)
            score = 2;
        else
            continue;
        if (IsEqualGUID(cfg->guidConfigBitstreamEncryption, DXVA_NoEncrypt))
            score += 16;

        if (cfg_score < score) {
            m_cfg = *cfg;
            cfg_score = score;
        }
    }
    CoTaskMemFree(cfg_list);
    if (cfg_score <= 0) {
        cl_log.log(cl_logERROR, "Failed to find a supported decoder configuration");
        return false;
    }

    /* Create the decoder */
    IDirectXVideoDecoder *decoder1;
    if (FAILED(m_vs->CreateVideoDecoder(
        m_input,
        &dsc,
        &m_cfg,
        surface_list,
        m_surfaceCount,
        &decoder1))) {
            cl_log.log(cl_logERROR, "IDirectXVideoDecoderService_CreateVideoDecoder failed\n");
            return false;
    }
    m_decoder = decoder1;
    cl_log.log(cl_logDEBUG1, "IDirectXVideoDecoderService_CreateVideoDecoder succeed");
    return true;
}

void DxvaSupportObject::DxCreateVideoConversion()
{
    switch (m_render) {
    case MAKEFOURCC('N','V','1','2'):
        m_output = (D3DFORMAT)MAKEFOURCC('Y','V','1','2');
        break;
    default:
        m_output = m_render;
        break;
    }
    CopyInitCache(&m_surfaceCache, m_surfaceWidth);
}

bool DxvaSupportObject::DxResetVideoDecoder()
{
    cl_log.log(cl_logERROR, "DxResetVideoDecoder unimplemented");
    return false;
}

/**
* It destroys a Direct3D device manager
*/
void DxvaSupportObject::D3dDestroyDeviceManager()
{
    if (m_devmng)
        m_devmng->Release();
}

DxvaSurface* DxvaSupportObject::grabUnusedSurface()
{
    /* Grab an unused surface, in case none are, try the oldest
    * XXX using the oldest is a workaround in case a problem happens with ffmpeg */
    unsigned i, old;
    for (i = 0, old = 0; i < m_surfaceCount; i++) {
        DxvaSurface *surface = &m_surface[i];

        if (!surface->refcount)
            break;

        if (surface->order < m_surface[old].order)
            old = i;
    }
    if (i >= m_surfaceCount)
        i = old;

    DxvaSurface *surface = &m_surface[i];
    surface->refcount = 1;
    surface->order = m_surfaceOrder++;

    return surface;
}

bool DxvaSupportObject::get(AVFrame *ff)
{
    if (!testDevice())
        return false;

    DxvaSurface *surface = grabUnusedSurface();

    for (int i = 0; i < 4; i++)
    {
        ff->data[i] = NULL;
        ff->linesize[i] = 0;

        if (i == 0 || i == 3)
            ff->data[i] = (uint8_t *)surface->d3d;/* Yummie */
    }

    return true;
}

void DxvaSupportObject::release(AVFrame *ff)
{
    LPDIRECT3DSURFACE9 d3d = (LPDIRECT3DSURFACE9)(uintptr_t)ff->data[3];

    for (unsigned i = 0; i < m_surfaceCount; i++)
    {
        DxvaSurface *surface = &m_surface[i];

        if (surface->d3d == d3d)
            surface->refcount--;
    }
}
bool DecoderContext::setup(void **hw, FOURCC *chroma, int width, int height)
{
    if (m_va->width() == width && m_va->height() == height && m_va->isDecoder())
        goto ok;

    if (m_pict != 0)
    {
        delete[] m_pict->p[0].p_pixels;
        delete m_pict;
        m_pict = 0;
    }

    /* */
    m_va->DxDestroyVideoConversion();
    m_va->DxDestroyVideoDecoder();

    *hw = NULL;
    *chroma = 0;
    if (width <= 0 || height <= 0)
        return false;

    /* FIXME transmit a video_format_t by VaSetup directly */
    //video_format_t fmt;
    //memset(&fmt, 0, sizeof(fmt));
    //fmt.i_width = width;
    //fmt.i_height = height;

    if (!m_va->DxCreateVideoDecoder(m_va->codecId(), width, height))
        return false;

    /* */
    m_va->initHw();

    /* */
    m_va->DxCreateVideoConversion();

    m_pict = new picture_t;
    memset(m_pict, 0, sizeof(picture_t));

    m_pict->p[0].p_pixels = new unsigned char[width * height * 3 / 2];
    m_pict->p[1].p_pixels = m_pict->p[0].p_pixels + width * height;
    m_pict->p[2].p_pixels = m_pict->p[1].p_pixels + width * height / 4;

    m_pict->p[0].i_pitch = width;
    m_pict->p[1].i_pitch = width / 2;
    m_pict->p[2].i_pitch = width / 2;

ok:
    *hw = &m_va->hw();
    const d3d_format_t *output = D3dFindFormat(m_va->output());
    *chroma = output->format;

    return true;
}

bool DecoderContext::extract(AVFrame *ff)
{
    LPDIRECT3DSURFACE9 d3d = (LPDIRECT3DSURFACE9)(uintptr_t)ff->data[3];

    if (!m_va->surfaceCache().buffer)
        return false;

    /* */
    // assert(m_Va->output == MAKEFOURCC('Y','V','1','2'));

    /* */
    D3DLOCKED_RECT lock;
    if (FAILED(d3d->LockRect(&lock, NULL, D3DLOCK_READONLY))) {
        cl_log.log(cl_logERROR, "Failed to lock surface");
        return false;
    }

    if (m_va->render() == MAKEFOURCC('Y','V','1','2')) {
        uint8_t *plane[3] = {
            (uint8_t*)lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * m_va->surfaceHeight(),
            (uint8_t*)lock.pBits + lock.Pitch * m_va->surfaceHeight()
            + (lock.Pitch/2) * (m_va->surfaceHeight()/2)
        };
        size_t  pitch[3] = {
            lock.Pitch,
            lock.Pitch / 2,
            lock.Pitch / 2,
        };
        CopyFromYv12(m_pict, plane, pitch,
            m_va->width(), m_va->height(),
            &m_va->surfaceCache());
    } else {
//        assert(m_Va->render == MAKEFOURCC('N','V','1','2'));
        uint8_t *plane[2] = {
            (uint8_t*)lock.pBits,
            (uint8_t*)lock.pBits + lock.Pitch * m_va->surfaceHeight()
        };
        size_t  pitch[2] = {
            lock.Pitch,
            lock.Pitch,
        };

        CopyFromNv12(m_pict, plane, pitch,
            m_va->width(), m_va->height(),
            &m_va->surfaceCache());
    }

    /* */
    d3d->UnlockRect();

    ff->data[0] = m_pict->p[0].p_pixels;
    ff->data[1] = m_pict->p[1].p_pixels;
    ff->data[2] = m_pict->p[2].p_pixels;

    ff->linesize[0] = m_pict->p[0].i_pitch;
    ff->linesize[1] = m_pict->p[1].i_pitch;
    ff->linesize[2] = m_pict->p[2].i_pitch;

    return true;
}

void DecoderContext::close()
{
    if (m_va)
    {
        m_va->close();

        delete m_va;
        m_va = 0;
    }

    if (m_pict != 0)
    {
        delete[] m_pict->p[0].p_pixels;
        delete m_pict;
        m_pict = 0;
    }
}

bool DecoderContext::newDxva(int codec_id)
{
    m_va = new DxvaSupportObject(codec_id);

    if (!m_va->newDxva())
    {
        close();
        return false;
    }

    return true;
}

bool DecoderContext::isHardwareAcceleration() const
{
    return m_hardwareAcceleration;
}

void DecoderContext::setHardwareAcceleration(bool hardwareAcceleration)
{
    m_hardwareAcceleration = hardwareAcceleration;
}

DxvaSupportObject* DecoderContext::va()
{
    return m_va;
}

picture_t* DecoderContext::pict()
{
    return m_pict;
}

#endif
