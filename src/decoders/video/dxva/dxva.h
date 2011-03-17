#ifndef universal_client_dxva_h_
#define universal_client_dxva_h_

#include "picture.h"
#include "copy.h"

#define VA_DXVA2_MAX_SURFACE_COUNT (64)

/**
  * Reference counted Direct3D surface
  */
struct DxvaSurface
{
    DxvaSurface()
        : d3d(0),
        refcount(0),
        order(0)
    {
    }

    LPDIRECT3DSURFACE9 d3d;
    int                refcount;
    unsigned int       order;
};

/**
  * Class for doing all low-level DXVA staff.
  */
class DxvaSupportObject
{
public:
    DxvaSupportObject(int codecId);

    bool newDxva();

    void initHw();
    void close();

    void release(AVFrame *ff);
    bool get(AVFrame *ff);

    // Getters
    int codecId() const { return m_codecId; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool isDecoder() const { return m_decoder != 0; }
    dxva_context& hw() { return m_hw; }
    copy_cache_t& surfaceCache() { return m_surfaceCache; }

    const D3DFORMAT output() const { return m_output; }
    const D3DFORMAT render() const { return m_render; }

    int surfaceHeight() const { return m_surfaceHeight; }

    // Setters
    const QString& description() const { return m_description; }

    void DxDestroyVideoConversion();
    void DxDestroyVideoDecoder();
    bool DxCreateVideoDecoder(int codec_id, int width, int height);
    void DxCreateVideoConversion();
private:
    bool testDevice();
    DxvaSurface* grabUnusedSurface();

    void DxDestroyVideoService();
    void D3dDestroyDevice();
    bool D3dCreateDevice();
    bool D3dCreateDeviceManager();
    bool DxCreateVideoService();
    bool DxFindVideoServiceConversion(GUID *input, D3DFORMAT *output);
    QString DxDescribe();
    bool DxResetVideoDecoder();
    void D3dDestroyDeviceManager();

    QString m_description;

    int          m_codecId;
    int          m_width;
    int          m_height;

    /* Direct3D */
    D3DPRESENT_PARAMETERS  m_d3dpp;
    LPDIRECT3D9            m_d3dobj;
    D3DADAPTER_IDENTIFIER9 m_d3dai;
    LPDIRECT3DDEVICE9      m_d3ddev;

    /* Device manager */
    UINT                     m_token;
    IDirect3DDeviceManager9  *m_devmng;
    HANDLE                   m_device;

    /* Video service */
    IDirectXVideoDecoderService  *m_vs;
    GUID                         m_input;
    D3DFORMAT                    m_render;

    /* Video decoder */
    DXVA2_ConfigPictureDecode    m_cfg;
    IDirectXVideoDecoder         *m_decoder;

    /* Option conversion */
    D3DFORMAT                    m_output;
    copy_cache_t                 m_surfaceCache;

    /* */
    dxva_context m_hw;

    /* */
    unsigned     m_surfaceCount;
    unsigned     m_surfaceOrder;
    int          m_surfaceWidth;
    int          m_surfaceHeight;
    FOURCC surface_chroma; // vlc_fourcc_t

    DxvaSurface m_surface[VA_DXVA2_MAX_SURFACE_COUNT];
    LPDIRECT3DSURFACE9 m_hwSurface[VA_DXVA2_MAX_SURFACE_COUNT];
};

/**
  * DXVA decoder context class. Lives with decoder.
  *
  * This class is intended to hold all dxva-related decoder context. The instance of is class is always passed to 
  * ffmpeg callbacks via 'opaque' pointer.
  *
  * Currently calss contains DXVA support object and a picture to store decoded information to. 
  */
class DecoderContext
{
public:
    DecoderContext()
        : m_va(0), m_pict(0),
        m_hardwareAcceleration(false) {}

    /**
      * Initialize dxva. Should be called once for a stream.
      * 
      * @return true on success, false otherwise
      */
    bool newDxva(int codec_id);

    /**
    * Setup dxva. Should be called once for each new frame.
    * 
    * @return true on success, false otherwise
    */
    bool setup(void **hw, FOURCC *chroma, int width, int height);

    /**
    * Extract image. Should be called each time after avcodec_decode_video call to fetch the image from video card.
    * 
    * @param ff picture returned from decoder
    * @return true on success, false otherwise
    */
    bool extract(AVFrame *ff);

    /**
    * Destroy dxva support object and picture.
    */
    void close();

    // Getters and setters
    DxvaSupportObject* va();
    picture_t* pict();

    bool isHardwareAcceleration() const;

    /**
      * Set hardware acceleration flag.
      */
    void setHardwareAcceleration(bool hardwareAcceleration);
private:
    DxvaSupportObject* m_va;
    picture_t* m_pict;

    bool m_hardwareAcceleration;
};

#endif // universal_client_dxva_h_
