#include "screen_grabber.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "base/colorspace_convert/colorspace.h"
#include "base/log.h"

typedef DECLSPEC_IMPORT HRESULT (STDAPICALLTYPE *DwmEnableComposition) (UINT uCompositionAction);


QMutex CLScreenGrapper::m_instanceMutex;
int CLScreenGrapper::m_aeroInstanceCounter;

class CPUDetector
{
public:
    enum CPUVersion { HAS_x86, HAS_MMX, HAS_SSE, HAS_SSE2, HAS_SSE3, HAS_SSE41, HAS_SSE42, HAS_AVX};

    CPUVersion version() const { return m_version; }

    CPUDetector()
    {
        int a,b,c,d;
        __asm {
            mov eax,1
	        cpuid
	        mov	a, eax
	        mov	b, ebx
	        mov	c, ecx
	        mov	d, edx
        }
        if (c & 1)
            m_version = HAS_SSE3;
        else if (d & (1<<26))
            m_version = HAS_SSE2;
        else if (d & (1<<25))
            m_version = HAS_SSE;
        else if (d & (1<<23))
            m_version = HAS_MMX;
        else
            m_version = HAS_x86;
    }
private:
    CPUVersion m_version;
};
CPUDetector cpuInfo;


void CLScreenGrapper::toggleAero(bool value)
{
    QLibrary lib("Dwmapi");
    if (lib.load())
    {
        DwmEnableComposition f = (DwmEnableComposition)lib.resolve("DwmEnableComposition");
        if (f)
            f(value ? 1 : 0);
    }
}

CLScreenGrapper::CLScreenGrapper(int displayNumber, int poolSize, CaptureMode mode, bool captureCursor, const QSize& captureResolution, QWidget* widget): 
    m_pD3D(0), 
    m_pd3dDevice(0), 
    m_displayNumber(displayNumber),
    m_frameNum(0),
    m_currentIndex(0),
    m_mode(mode),
    m_poolSize(poolSize+1),
    m_captureCursor(captureCursor),
    m_captureResolution(captureResolution),
    m_scaleContext(0),
    m_tmpFrame(0),
    m_tmpFrameBuffer(0),
    m_widget(widget),
    m_tmpFrameWidth(0),
    m_tmpFrameHeight(0)
{
    memset(&m_rect, 0, sizeof(m_rect));

    if (m_mode == CaptureMode_DesktopWithoutAero)
    {
        QMutexLocker locker(&m_instanceMutex);
        if (++m_aeroInstanceCounter == 1)
            toggleAero(false);
    }
    m_needRescale = captureResolution.width() != 0 || mode == CaptureMode_Application;
    if (mode == CaptureMode_Application)
        m_displayNumber = 0;

    m_initialized = InitD3D(GetDesktopWindow());
    m_cursorDC = CreateCompatibleDC(0);
    m_timer.start();
}

CLScreenGrapper::~CLScreenGrapper()
{
    for (int i = 0; i < m_openGLData.size(); ++i)
        delete [] m_openGLData[i];

    for (int i = 0; i < m_pSurface.size(); ++i)
    {
        if(m_pSurface[i])
        {
            m_pSurface[i]->Release();
            m_pSurface[i]=NULL;
        }
    }
    if(m_pd3dDevice)
    {
        m_pd3dDevice->Release();
        m_pd3dDevice=NULL;
    }
    if(m_pD3D)
    {
        m_pD3D->Release();
        m_pD3D=NULL;
    }
    if (m_mode == CaptureMode_DesktopWithoutAero)
    {
        QMutexLocker locker(&m_instanceMutex);
        if (--m_aeroInstanceCounter == 0)
            toggleAero(true);
    }
    DeleteDC(m_cursorDC);

    if (m_scaleContext)
    {
        sws_freeContext(m_scaleContext);
        av_free(m_tmpFrame);
        av_free(m_tmpFrameBuffer);
    }
}

HRESULT	CLScreenGrapper::InitD3D(HWND hWnd)
{
    D3DPRESENT_PARAMETERS	d3dpp;

    if((m_pD3D=Direct3DCreate9(D3D_SDK_VERSION))==NULL)
    {
        cl_log.log("Unable to Create Direct3D ", cl_logERROR);
        return E_FAIL;
    }

    memset(&m_monInfo, 0, sizeof(m_monInfo));
    m_monInfo.cbSize = sizeof(m_monInfo);
    if (!GetMonitorInfo(m_pD3D->GetAdapterMonitor(m_displayNumber), &m_monInfo))
    {
        cl_log.log("Unable to determine monitor position. Use default", cl_logWARNING);
    }

    if(FAILED(m_pD3D->GetAdapterDisplayMode(m_displayNumber,&m_ddm)))
    {
        cl_log.log("Unable to Get Adapter Display Mode", cl_logERROR);
        return E_FAIL;
    }

    ZeroMemory(&d3dpp,sizeof(D3DPRESENT_PARAMETERS));

    d3dpp.Windowed=true; // not fullscreen
    d3dpp.Flags = 0; //D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    d3dpp.BackBufferFormat = m_ddm.Format;
    d3dpp.BackBufferHeight = m_rect.bottom = m_ddm.Height;
    d3dpp.BackBufferWidth = m_rect.right = m_ddm.Width;
    d3dpp.MultiSampleType=D3DMULTISAMPLE_NONE;
    d3dpp.BackBufferCount = 1;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect=D3DSWAPEFFECT_COPY;
    d3dpp.hDeviceWindow=hWnd;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // D3DPRESENT_INTERVAL_DEFAULT;
    d3dpp.FullScreen_RefreshRateInHz=D3DPRESENT_RATE_DEFAULT;


    if (m_mode != CaptureMode_Application)
    {
        if(FAILED(m_pD3D->CreateDevice(m_displayNumber, D3DDEVTYPE_HAL,hWnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING ,&d3dpp,&m_pd3dDevice)))
        {
            qWarning() << "Unable to Create Device";
            return E_FAIL;
        }
        m_pSurface.resize(m_poolSize);
        for (int i = 0; i < m_pSurface.size(); ++i)
        {
            if(FAILED(m_pd3dDevice->CreateOffscreenPlainSurface(m_ddm.Width, m_ddm.Height, D3DFMT_A8R8G8B8, 
                D3DPOOL_SCRATCH, 
                &m_pSurface[i], NULL)))
            {
                qWarning() << "Unable to Create Surface";
                return E_FAIL;
            }
        }
    }
    else
    {
        for (int i = 0; i < m_poolSize; ++i)
            m_openGLData << new quint8[m_rect.right * m_rect.bottom * 4];
    }

    m_outWidth = m_ddm.Width;
    m_outHeight = m_ddm.Height;
    if (m_needRescale)
    {
        if (m_captureResolution.width() < 0)
            m_outWidth =  m_ddm.Width / qAbs(m_captureResolution.width()); 
        else if (m_captureResolution.width() == 0)
            m_outWidth =  m_ddm.Width; 
        else
            m_outWidth = m_captureResolution.width(); 

        if (m_captureResolution.height() < 0)
            m_outHeight =  m_ddm.Height / qAbs(m_captureResolution.height()); 
        else if (m_captureResolution.height() == 0)
            m_outHeight =  m_ddm.Height; 
        else
            m_outHeight = m_captureResolution.height(); 

        allocateTmpFrame(m_ddm.Width, m_ddm.Height);

    }

    return S_OK;
};

void CLScreenGrapper::allocateTmpFrame(int width, int height)
{
    m_tmpFrameWidth = width;
    m_tmpFrameHeight = height;

    if (m_tmpFrame)
    {
        sws_freeContext(m_scaleContext);
        av_free(m_tmpFrame);
        av_free(m_tmpFrameBuffer);
        m_tmpFrame = 0;
    }

    m_tmpFrame = avcodec_alloc_frame();
    m_tmpFrame->format = PIX_FMT_YUV420P;
    int numBytes = avpicture_get_size(PIX_FMT_YUV420P, width, height);
    m_tmpFrameBuffer = (quint8*)av_malloc(numBytes * sizeof(quint8));
    avpicture_fill((AVPicture *)m_tmpFrame, m_tmpFrameBuffer, PIX_FMT_YUV420P, width, height);

    m_scaleContext = sws_getContext(
        width, height, PIX_FMT_YUV420P,
        m_outWidth, m_outHeight, PIX_FMT_YUV420P,
        SWS_BICUBLIN, NULL, NULL, NULL);
}

void CLScreenGrapper::captureFrameOpenGL(void* opaque)
{
    CaptureInfo* data = (CaptureInfo*) opaque;
    glReadBuffer(GL_FRONT); 
    if (!m_widget)
        return;
    QRect rect = m_widget->geometry();
    data->w = m_widget->width() & ~31;
    data->h = m_widget->height() & ~1;
    data->pos = QPoint(rect.left(), rect.top());
    QWidget* parent = (QWidget*) m_widget->parent();
    while (parent)
    {
        rect = parent->geometry();
        data->pos += QPoint(rect.left(), rect.top());
        parent = (QWidget*) parent->parent();
    }

    glReadPixels(0, 0, data->w, data->h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data->opaque);
}

CLScreenGrapper::CaptureInfo CLScreenGrapper::captureFrame()
{
    CaptureInfo rez;

    if (m_mode == CaptureMode_Application) 
    {   
        rez.opaque = m_openGLData[m_currentIndex];
        QGenericReturnArgument ret;
        QMetaObject::invokeMethod(this, "captureFrameOpenGL", Qt::BlockingQueuedConnection, ret, Q_ARG(void*, &rez));
        if (m_captureCursor)
            drawCursor((quint32*) rez.opaque, rez.w, rez.h, rez.pos.x(), rez.pos.y(), true);

    }
    else 
    {   // direct3D capture mode
        if (!m_pd3dDevice)
            return rez;
        HRESULT r = m_pd3dDevice->GetFrontBufferData(0, m_pSurface[m_currentIndex]);
        if(r != D3D_OK)
        {
            cl_log.log("Unable to capture frame", cl_logWARNING);
            return rez;
        }
        if (m_captureCursor)
        {
            D3DLOCKED_RECT	lockedRect;
            if(m_pSurface[m_currentIndex]->LockRect(&lockedRect, &m_rect, D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY) == D3D_OK)
            {
                drawCursor((quint32*) lockedRect.pBits, m_ddm.Width, m_ddm.Height, m_monInfo.rcMonitor.left, m_monInfo.rcMonitor.top, false);
                m_pSurface[m_currentIndex]->UnlockRect();
            }
        }
        rez.opaque = m_pSurface[m_currentIndex];
    }
    rez.pts = m_timer.elapsed();
    m_currentIndex = m_currentIndex < m_pSurface.size()-1 ? m_currentIndex+1 : 0;
    return rez;
}

void bgra_to_yv12_sse(quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height, bool flip)
{
    static const quint16 __declspec(align(16)) sse_2000[]         = { 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020, 0x2020 };
    static const quint16 __declspec(align(16)) sse_00a0[]         = { 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210, 0x0210 };
    static const quint32 __declspec(align(16)) sse_mask_color[]   = { 0x00003fc0, 0x00003fc0, 0x00003fc0, 0x00003fc0};
    static const qint16 __declspec(align(16)) sse_01[] = {0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001};

    static const int K = 32768;
    static const qint16 __declspec(align(16)) y_r_coeff[] =  { 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K, 0.257*K };
    static const qint16 __declspec(align(16)) y_g_coeff[] =  { 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K, 0.504*K };
    static const qint16 __declspec(align(16)) y_b_coeff[] =  { 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K, 0.098*K };

    static const qint16 __declspec(align(16)) uv_r_coeff[] = { 0.439*K,  0.439*K,  0.439*K,  0.439*K,    -0.148*K, -0.148*K, -0.148*K, -0.148*K };
    static const qint16 __declspec(align(16)) uv_g_coeff[] = {-0.368*K, -0.368*K, -0.368*K, -0.368*K,    -0.291*K, -0.291*K, -0.291*K, -0.291*K };
    static const qint16 __declspec(align(16)) uv_b_coeff[] = {-0.071*K, -0.071*K, -0.071*K, -0.071*K,     0.439*K,  0.439*K,  0.439*K,  0.439*K };

    int xCount = width  / 8;

    int rgbaPostOffset = xStride - xCount*32;
    int yPostOffset = yStride - width;
    int uvPostOffset = uvStride - width/2;

    if (flip)
    {
        y += yStride*(height-1);
        u += uvStride*(height/2-1);
        v += uvStride*(height/2-1);
        yPostOffset -= yStride*2;
        uvPostOffset -= uvStride*2;
        yStride = -yStride;
        uvStride = -uvStride;
    }
    height /= 2;


    // EAX - u
    // ECX - v
    // edx - RGBA stride in bytes
    // ebx = Y stride
    // esi - source RGB
    // edi - Y
    __asm 
    {
        mov esi, rgba
        mov edi, y
        mov eax, u
        mov edx, xStride
        mov ebx, yStride
        mov ecx, v
loop1:
        PREFETCHNTA [esi + 64]
        PREFETCHNTA [esi+edx + 64]

        // prepare first line
        MOVAPS    xmm0, [esi] // BGRA
        MOVAPS    xmm6, [esi+16] // BGRA

        MOVAPS    xmm1, [esi]
        MOVAPS    xmm7, [esi+16]

        psllD xmm0, 6 // R
        psllD xmm6, 6 // R

        psrlD xmm1, 2 // G
        psrlD xmm7, 2 // G

        pand xmm0, sse_mask_color
        pand xmm6, sse_mask_color

        pand xmm1, sse_mask_color
        pand xmm7, sse_mask_color

        packusdw xmm0,xmm6 // pack dword to word
        packusdw xmm1,xmm7

        MOVAPS    xmm5, [esi]
        MOVAPS    xmm6, [esi+16]

        MOVAPS    xmm3, [esi+edx] // BGRA
        MOVAPS    xmm7, [esi+edx+16] // BGRA

        psrlD xmm5, 10 // B
        psrlD xmm6, 10 // B

        psrlD xmm3, 2 // G
        psrlD xmm7, 2 // G

        pand xmm5, sse_mask_color
        pand xmm6, sse_mask_color

        pand xmm3, sse_mask_color
        pand xmm7, sse_mask_color

        packusdw xmm5,xmm6
        packusdw xmm3,xmm7

        // prepare second line
        // -------------------------------------------------------------------------------------------

        // 2,5 - for B
        MOVAPS    xmm2, [esi+edx]
        MOVAPS    xmm6, [esi+edx+16]

        MOVAPS    xmm4, [esi+edx]
        MOVAPS    xmm7, [esi+edx+16]

        psllD xmm2, 6 // R
        psllD xmm6, 6 // R

        psrlD xmm4, 10 // B
        psrlD xmm7, 10 // B

        pand xmm2, sse_mask_color
        pand xmm6, sse_mask_color

        pand xmm4, sse_mask_color
        pand xmm7, sse_mask_color

        packusdw xmm2,xmm6 // pack dword to word
        packusdw xmm4,xmm7

        // ---------------------------------------------------------------------------------------

        MOVAPS xmm6, xmm0
        MOVAPS xmm7, xmm1

        // 0,1,5 - first line. 2,3,4 - second

        // calculate Y of first line
        pmulhw xmm6, y_b_coeff
        pmulhw xmm7, y_g_coeff

        pavgw xmm0, xmm2
        paddw xmm6, xmm7

        MOVAPS xmm7, xmm5

        paddw xmm6, sse_00a0
        pmulhw xmm7, y_r_coeff

        pavgw xmm1, xmm3
        paddw xmm6, xmm7

        pavgw xmm5, xmm4
        psrlw xmm6, 5

        // ----------------------------------------------

        MOVAPS xmm7, xmm3
        packuswb xmm6, xmm6

        pmulhw xmm7, y_g_coeff
        MOVLPS [edi], xmm6

        // calculate Y of second line
        MOVAPS xmm6, xmm2
        PMADDWD xmm1, sse_01

        pmulhw xmm6, y_b_coeff
        PMADDWD xmm0, sse_01

        paddw xmm6, xmm7
        MOVAPS xmm7, xmm4
                        
        PMADDWD xmm5, sse_01
        pmulhw xmm7, y_r_coeff

        paddw xmm6, sse_00a0
        packusdw xmm0, xmm0

        paddw xmm6, xmm7
        packusdw xmm1, xmm1

        psrlw xmm6, 5
        packusdw xmm5, xmm5

        pmulhw xmm0, uv_b_coeff
        packuswb xmm6, xmm6

        pmulhw xmm1, uv_g_coeff
        MOVLPS [edi+ebx], xmm6

        // ---------------------------------------------------------
        // calculate UV colors


        paddw xmm0, sse_2000
        pmulhw xmm5, uv_r_coeff

        paddw xmm0, xmm1
        add esi, 32

        paddw xmm0, xmm5
        add edi, 8

        psrlw xmm0, 6
        packuswb xmm0, xmm0

        MOVD [ecx], xmm0
        psrlq xmm0, 32

        add ecx, 4
        MOVD [eax], xmm0
        
        add eax, 4
        dec xCount
        jnz loop1

        // ------------------ Y loop
        push width
        pop xCount
        shr xCount, 3

        add esi, edx
        add edi, ebx

        add esi, rgbaPostOffset
        add edi, yPostOffset

        add eax, uvPostOffset
        add ecx, uvPostOffset

        dec height
        jnz loop1
    }
}

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height)
{
    int xStride = width*4;
    quint8* yptr2 = yptr + width;
    quint8* rgba2 = rgba + xStride;
    for (int y = height/2; y > 0; --y)
    {
        for (int x = width/2; x > 0; --x)
        {
            // process 4 Y pixels
            quint8 y1 = rgba[0]*0.098f + rgba[1]*0.504f + rgba[2]*0.257f + 16.5f;
            *yptr++ = y1;
            quint8 y2 = rgba[4]*0.098f + rgba[5]*0.504f + rgba[6]*0.257f + 16.5f;
            *yptr++ = y2;

            quint8 y3 = rgba2[0]*0.098 + rgba2[1]*0.504 + rgba2[2]*0.257 + 16.5f;
            *yptr2++ = y3;
            quint8 y4 = rgba2[4]*0.098 + rgba2[5]*0.504 + rgba2[6]*0.257 + 16.5f;
            *yptr2++ = y4;
            
            // calculate avarage RGB values for UV pixels
            quint8 avgB = (rgba[0]+rgba[4]+rgba2[0]+rgba2[4]+2) >> 2;
            quint8 avgG = (rgba[1]+rgba[5]+rgba2[1]+rgba2[5]+2) >> 2;
            quint8 avgR = (rgba[2]+rgba[6]+rgba2[2]+rgba2[6]+2) >> 2;

            // write UV pixels
            *vptr++ = 0.439f*avgR  - 0.368f*avgG - 0.071f*avgB + 128.5f;
            *uptr++ = -0.148f*avgR - 0.291f*avgG + 0.439f*avgB + 128.5f;

            rgba += 8;
            rgba2 += 8;
        }
        rgba   += xStride;
        rgba2  += xStride;
        yptr  += width;
        yptr2 += width;
    }
}

static inline void andPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y*stride+x] &= *srcPixel;
}

static inline void xorPixel(quint32* data, int stride, int x, int y, quint32* srcPixel)
{
    data[y*stride+x] ^= *srcPixel;
}

void CLScreenGrapper::drawCursor(quint32* data, int width, int height, int leftOffset, int topOffset, bool flip) const
{
    static const int MAX_CURSOR_SIZE = 64;
    CURSORINFO  pci;
    pci.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&pci))
    {
        if (! (pci.flags & CURSOR_SHOWING))
            return;

        ICONINFO icInfo;
        memset(&icInfo, 0, sizeof(ICONINFO));
        if (GetIconInfo(pci.hCursor, &icInfo))
        {
            int xPos = pci.ptScreenPos.x - ((int)icInfo.xHotspot);
            int yPos = pci.ptScreenPos.y - ((int)icInfo.yHotspot);

            xPos -= leftOffset;
            yPos -= topOffset;

            if (yPos >= 0 && yPos < height && flip)
                yPos = height - yPos;

            quint8 maskBits[MAX_CURSOR_SIZE*MAX_CURSOR_SIZE*2/8 * 16];
            quint8 colorBits[MAX_CURSOR_SIZE*MAX_CURSOR_SIZE*4]; // max cursor size: 64x64
            quint8 lpbiData[sizeof(BITMAPINFO) + 4]; // 2 additional colors
            quint8 lpbiColorData[sizeof(BITMAPINFO) + 3*4]; // 4 additional colors

            BITMAPINFO* lpbi = (BITMAPINFO*) &lpbiData;
            BITMAPINFO* lpbiColor = (BITMAPINFO*) &lpbiColorData;
            memset(lpbi, 0, sizeof(BITMAPINFO));
            memset(lpbiColor, 0, sizeof(BITMAPINFO));
            lpbi->bmiHeader.biSize = sizeof(BITMAPINFO);
            lpbiColor->bmiHeader.biSize = sizeof(BITMAPINFO);
            if (GetDIBits(m_cursorDC, icInfo.hbmMask, 0, 0, 0, lpbi, 0) == 0)
            {
                DeleteObject(icInfo.hbmMask);
                DeleteObject(icInfo.hbmColor);
                return;
            }
            if (lpbi->bmiHeader.biClrUsed > 2 || GetDIBits(m_cursorDC, icInfo.hbmMask, 0, lpbi->bmiHeader.biHeight, maskBits, lpbi, 0) == 0)
            {
                DeleteObject(icInfo.hbmMask);
                DeleteObject(icInfo.hbmColor);
                return;
            }


            quint8* maskBitsPtr = maskBits;
            int bitmaskStride = lpbi->bmiHeader.biSizeImage / lpbi->bmiHeader.biHeight;

            // and mask

            quint8 maskFlag = icInfo.hbmColor ? 0 : 0x80;

            int cursorHeight = lpbi->bmiHeader.biHeight;
            if (icInfo.hbmColor == 0)
                cursorHeight /= 2;

            if (!flip)
            {
                for (int y = yPos + cursorHeight-1; y >= yPos; --y) 
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                    {
                        quint8 mask = *maskBitsPtr++;
                        for (int k = 0; k < 8; ++k)
                        {
                            if ((mask & 0x80) == maskFlag && x > 0 && y > 0 && x < width && y < height)
                                andPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[0]);
                            mask <<= 1;
                            x++;
                        }
                    }
                    maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                }
            }
            else 
            {
                for (int y = yPos; y < yPos + cursorHeight; ++y) 
                {
                    for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                    {
                        quint8 mask = *maskBitsPtr++;
                        for (int k = 0; k < 8; ++k)
                        {
                            if ((mask & 0x80) == maskFlag && x > 0 && y > 0 && x < width && y < height)
                                andPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[0]);
                            mask <<= 1;
                            x++;
                        }
                    }
                    maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                }
            }
            if (icInfo.hbmColor)
            {
                if (GetDIBits(m_cursorDC, icInfo.hbmColor, 0, 0, 0, lpbiColor, 0) == 0)
                {
                    DeleteObject(icInfo.hbmMask);
                    DeleteObject(icInfo.hbmColor);
                    return;
                }

                if (GetDIBits(m_cursorDC, icInfo.hbmColor, 0, lpbiColor->bmiHeader.biHeight, colorBits, lpbiColor, 0) == 0)
                {
                    DeleteObject(icInfo.hbmMask);
                    DeleteObject(icInfo.hbmColor);
                    return;
                }

                quint32* colorBitsPtr = (quint32*) colorBits;
                int colorStride = lpbiColor->bmiHeader.biSizeImage / lpbiColor->bmiHeader.biHeight;


                // draw xor colored cursor
                if (!flip)
                {
                    for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            for (int k = 0; k < 8; ++k)
                            {
                                if (x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, colorBitsPtr);
                                colorBitsPtr++;
                                x++;
                            }
                        }
                        colorBitsPtr += colorStride - lpbiColor->bmiHeader.biWidth/8 * lpbiColor->bmiHeader.biBitCount;
                    }
                }
                else 
                {
                    for (int y = yPos; y < yPos + cursorHeight; ++y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            for (int k = 0; k < 8; ++k)
                            {
                                if (x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, colorBitsPtr);
                                colorBitsPtr++;
                                x++;
                            }
                        }
                        colorBitsPtr += colorStride - lpbiColor->bmiHeader.biWidth/8 * lpbiColor->bmiHeader.biBitCount;
                    }
                }
            }
            else 
            {
                // draw xor cursor
                if (!flip)
                {
                    for (int y = yPos + cursorHeight-1; y >= yPos; --y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            quint8 mask = *maskBitsPtr++;
                            for (int k = 0; k < 8; ++k)
                            {
                                if ((mask & 0x80) == 0 && x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[1]);
                                mask <<= 1;
                                x++;
                            }
                        }
                        maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                    }
                }
                else 
                {
                    for (int y = yPos; y < yPos + cursorHeight; ++y)
                    {
                        for (int x = xPos; x < xPos + lpbi->bmiHeader.biWidth; )
                        {
                            quint8 mask = *maskBitsPtr++;
                            for (int k = 0; k < 8; ++k)
                            {
                                if ((mask & 0x80) == 0 && x > 0 && y > 0 && x < width && y < height)
                                    xorPixel(data, width, x, y, (quint32*) &lpbi->bmiColors[1]);
                                mask <<= 1;
                                x++;
                            }
                        }
                        maskBitsPtr += bitmaskStride - lpbi->bmiHeader.biWidth/8;
                    }
                }
            }
            DeleteObject(icInfo.hbmMask);
            DeleteObject(icInfo.hbmColor);
        }
    }
}

bool CLScreenGrapper::direct3DDataToFrame(void* opaque, AVFrame* pFrame)
{
    IDirect3DSurface9* pSurface = (IDirect3DSurface9*) opaque;

    D3DLOCKED_RECT	lockedRect;

    if(FAILED(pSurface->LockRect(&lockedRect, 0, D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY)))
    {
        qWarning() << "Unable to Lock Front Buffer Surface";
        return false;
    }

    if (pFrame->width == 0)
    {
        int videoBufSize = avpicture_get_size(format(), width(), height());
        quint8* videoBuf = (quint8*) av_malloc(videoBufSize);

        avpicture_fill((AVPicture *)pFrame, videoBuf, format(), width(), height());
        pFrame->key_frame = 1;
        //pFrame->pict_type = AV_PICTURE_TYPE_I;
        pFrame->width = m_ddm.Width;
        pFrame->height = m_ddm.Height;
        pFrame->format = format();
        pFrame->sample_aspect_ratio.num = 1;
        pFrame->sample_aspect_ratio.den = 1;

    }
    pFrame->coded_picture_number = m_frameNum++;
    //pFrame->pts = m_timer.elapsed();
    pFrame->best_effort_timestamp = pFrame->pts;

#if 0
    // speed test
    QTime t1;
    t1.start();
    for (int i = 0; i < 1000; ++i)
    {
        bgra_to_yv12_mmx((unsigned char*)lockedRect.pBits, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
            pFrame->linesize[0], pFrame->linesize[1], m_ddm.Width, m_ddm.Height, 0);
        //bgra_yuv420((unsigned char*)lockedRect.pBits, pFrame->data[0], pFrame->data[1], pFrame->data[2], m_ddm.Width, m_ddm.Height);
    }
    int time1 = t1.elapsed();

    QTime t2;
    t2.start();
    for (int i = 0; i < 1000; ++i)

    {
        bgra_to_yv12_sse((unsigned char*)lockedRect.pBits, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
            pFrame->linesize[0], pFrame->linesize[1], m_ddm.Width, m_ddm.Height);
    }
    int time2 = t2.elapsed();
    qDebug() << "time1=" << time1 << "time2=" << time2 << "t1/t2=" << time1 / (float) time2;
#else    
    bool rez = dataToFrame((unsigned char*)lockedRect.pBits, m_ddm.Width, m_ddm.Height, pFrame);
#endif
    pSurface->UnlockRect();
    return rez;
}

bool CLScreenGrapper::dataToFrame(quint8* data, int width, int height, AVFrame* pFrame)
{   
    if (m_needRescale)
    {
        if (width != m_tmpFrameWidth || height != m_tmpFrameHeight)
            allocateTmpFrame(width, height);
        bgra_to_yv12_sse(data, width * 4, 
            m_tmpFrame->data[0], m_tmpFrame->data[1], m_tmpFrame->data[2], 
            m_tmpFrame->linesize[0], m_tmpFrame->linesize[1], width, height, m_mode == CaptureMode_Application);

        sws_scale(m_scaleContext, 
            m_tmpFrame->data, m_tmpFrame->linesize, 
            0, m_ddm.Height,
            pFrame->data, pFrame->linesize);
    }
    else
    {
        bgra_to_yv12_sse(data, m_ddm.Width*4, pFrame->data[0], pFrame->data[1], pFrame->data[2], 
                         pFrame->linesize[0], pFrame->linesize[1], width, height, m_mode == CaptureMode_Application);
    }
   return true;
}

bool CLScreenGrapper::capturedDataToFrame(const CaptureInfo& captureInfo, AVFrame* pFrame)
{
    bool rez = false;
    if (m_mode == CaptureMode_Application)
        rez = dataToFrame((quint8*) captureInfo.opaque, captureInfo.w, captureInfo.h, pFrame);
    else 
        rez = direct3DDataToFrame(captureInfo.opaque, pFrame);
    pFrame->pts = captureInfo.pts;
    return rez;
}

qint64 CLScreenGrapper::currentTime() const
{
    return m_timer.elapsed();
}

int CLScreenGrapper::width() const          
{ 
    return m_outWidth;
}

int CLScreenGrapper::height() const         
{ 
    return m_outHeight;
}

