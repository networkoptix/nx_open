// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#if defined(__x86_64__)

#include "hw_video_api.h"

#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>

#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/private/qabstractvideobuffer_p.h>
#include <QtMultimedia/private/qvideotexturehelper_p.h>
#include <QtMultimedia/QVideoFrame>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <va/va.h>
#include <va/va_dri2.h>
#include <va/va_drmcommon.h>

#include "nx/utils/scope_guard.h"
#include "texture_helper.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
} // extern "C"

namespace nx::media {

namespace {

bool hasGlExtension(const char* extensions, const char* ext)
{
    const int len = strlen(ext);
    const char* cur = extensions;
    while (cur)
    {
        cur = strstr(cur, ext);
        if (!cur)
            break;
        if ((cur == extensions || cur[-1] == ' ')
            && (cur[len] == '\0' || cur[len] == ' '))
        {
            return true;
        }
        cur += len;
    }
    return false;
}

class TextureConverter
{
public:
    TextureConverter(QRhi* rhi):
        m_rhi(rhi)
    {
        if (!rhi || rhi->backend() != QRhi::OpenGLES2)
        {
            m_rhi = nullptr;
            return;
        }

        auto nativeHandles = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles());
        m_glContext = nativeHandles->context;
        if (!m_glContext)
        {
            m_rhi = nullptr;
            return;
        }

        const QString platform = QGuiApplication::platformName();
        QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
        m_eglDisplay = pni->nativeResourceForIntegration(QByteArrayLiteral("egldisplay"));

        if (!m_eglDisplay)
        {
            NX_WARNING(this, "no egl display, disabling");
            m_rhi = nullptr;
            return;
        }
        // TODO: check "glEGLImageTargetTexStorageEXT" for desktop OpenGL
        m_eglImageTargetTexture2D = eglGetProcAddress("glEGLImageTargetTexture2DOES");
        if (!m_eglImageTargetTexture2D)
        {
            NX_WARNING(this, "no eglImageTargetTexture2D display, disabling");
            m_rhi = nullptr;
            return;
        }

        if (!eglGetCurrentContext())
            return;

        const char* extensions = eglQueryString(m_eglDisplay, EGL_EXTENSIONS);

        // TODO: check
        // isOpenGLES ? "GL_OES_EGL_image" : "GL_EXT_EGL_image_storage";
        // "EGL_EXT_image_dma_buf_import"

        QOpenGLFunctions functions(m_glContext);
        NX_DEBUG(this, "EGL extensions: %1", extensions);
        m_useModifiers = hasGlExtension(extensions, "EGL_EXT_image_dma_buf_import_modifiers");
    }

    bool isEgl() const
    {
        return (bool) m_eglDisplay;
    }

    bool useModifiers() const
    {
        return m_useModifiers;
    }

    QRhi* m_rhi = nullptr;
    Qt::HANDLE m_eglDisplay = nullptr;
    QOpenGLContext* m_glContext = nullptr;
    QFunctionPointer m_eglImageTargetTexture2D = nullptr;
    bool m_useModifiers = false;
};

std::shared_ptr<TextureConverter> getTextureConverterForRhi(QRhi* rhi)
{
    static std::mutex mutex;
    static QHash<QRhi*, std::shared_ptr<TextureConverter>> allRhis;

    std::lock_guard lock(mutex);

    if (auto converter = allRhis.value(rhi, {}))
        return converter;

    std::shared_ptr<TextureConverter> converter = std::make_shared<TextureConverter>(rhi);

    rhi->addCleanupCallback(
        [](QRhi* rhi)
        {
            std::lock_guard lock(mutex);
            allRhis.remove(rhi);
        });

    allRhis[rhi] = converter;

    return converter;
}

// Do not use libdrm headers to avoid conflicts with Qt headers.

#define fourcc_code(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
                                 ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define DRM_FORMAT_RGBA8888     fourcc_code('R', 'A', '2', '4') /* [31:0] R:G:B:A 8:8:8:8 little endian */
#define DRM_FORMAT_RGB888       fourcc_code('R', 'G', '2', '4') /* [23:0] R:G:B little endian */
#define DRM_FORMAT_RG88         fourcc_code('R', 'G', '8', '8') /* [15:0] R:G 8:8 little endian */
#define DRM_FORMAT_ABGR8888     fourcc_code('A', 'B', '2', '4') /* [31:0] A:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_BGR888       fourcc_code('B', 'G', '2', '4') /* [23:0] B:G:R little endian */
#define DRM_FORMAT_GR88         fourcc_code('G', 'R', '8', '8') /* [15:0] G:R 8:8 little endian */
#define DRM_FORMAT_R8           fourcc_code('R', '8', ' ', ' ') /* [7:0] R */
#define DRM_FORMAT_R16          fourcc_code('R', '1', '6', ' ') /* [15:0] R little endian */
#define DRM_FORMAT_RGB565       fourcc_code('R', 'G', '1', '6') /* [15:0] R:G:B 5:6:5 little endian */
#define DRM_FORMAT_RG1616       fourcc_code('R', 'G', '3', '2') /* [31:0] R:G 16:16 little endian */
#define DRM_FORMAT_GR1616       fourcc_code('G', 'R', '3', '2') /* [31:0] G:R 16:16 little endian */
#define DRM_FORMAT_BGRA1010102  fourcc_code('B', 'A', '3', '0') /* [31:0] B:G:R:A 10:10:10:2 little endian */
#define DRM_FORMAT_MOD_INVALID  0x00ffffffffffffffULL

static const quint32 *fourccFromPixelFormat(const QVideoFrameFormat::PixelFormat format)
{
    #if Q_BYTE_ORDER == Q_BIG_ENDIAN
        const quint32 rgba_fourcc = DRM_FORMAT_RGBA8888;
        const quint32 rg_fourcc = DRM_FORMAT_RG88;
        const quint32 rg16_fourcc = DRM_FORMAT_RG1616;
    #else
        const quint32 rgba_fourcc = DRM_FORMAT_ABGR8888;
        const quint32 rg_fourcc = DRM_FORMAT_GR88;
        const quint32 rg16_fourcc = DRM_FORMAT_GR1616;
    #endif

    switch (format)
    {
        case QVideoFrameFormat::Format_Invalid:
        case QVideoFrameFormat::Format_IMC1:
        case QVideoFrameFormat::Format_IMC2:
        case QVideoFrameFormat::Format_IMC3:
        case QVideoFrameFormat::Format_IMC4:
        case QVideoFrameFormat::Format_SamplerExternalOES:
        case QVideoFrameFormat::Format_Jpeg:
        case QVideoFrameFormat::Format_SamplerRect:
            return nullptr;

        case QVideoFrameFormat::Format_ARGB8888:
        case QVideoFrameFormat::Format_ARGB8888_Premultiplied:
        case QVideoFrameFormat::Format_XRGB8888:
        case QVideoFrameFormat::Format_BGRA8888:
        case QVideoFrameFormat::Format_BGRA8888_Premultiplied:
        case QVideoFrameFormat::Format_BGRX8888:
        case QVideoFrameFormat::Format_ABGR8888:
        case QVideoFrameFormat::Format_XBGR8888:
        case QVideoFrameFormat::Format_RGBA8888:
        case QVideoFrameFormat::Format_RGBX8888:
        case QVideoFrameFormat::Format_AYUV:
        case QVideoFrameFormat::Format_AYUV_Premultiplied:
        case QVideoFrameFormat::Format_UYVY:
        case QVideoFrameFormat::Format_YUYV:
        {
            static constexpr quint32 format[] = {rgba_fourcc, 0, 0, 0};
            return format;
        }

        case QVideoFrameFormat::Format_Y8:
        {
            static constexpr quint32 format[] = {DRM_FORMAT_R8, 0, 0, 0};
            return format;
        }
        case QVideoFrameFormat::Format_Y16:
        {
            static constexpr quint32 format[] = {DRM_FORMAT_R16, 0, 0, 0};
            return format;
        }

        case QVideoFrameFormat::Format_YUV420P:
        case QVideoFrameFormat::Format_YUV422P:
        case QVideoFrameFormat::Format_YV12:
        {
            static constexpr quint32 format[] = {DRM_FORMAT_R8, DRM_FORMAT_R8, DRM_FORMAT_R8, 0};
            return format;
        }
        case QVideoFrameFormat::Format_YUV420P10:
        {
            static constexpr quint32 format[] = {DRM_FORMAT_R16, DRM_FORMAT_R16, DRM_FORMAT_R16, 0};
            return format;
        }

        case QVideoFrameFormat::Format_NV12:
        case QVideoFrameFormat::Format_NV21:
        {
            static constexpr quint32 format[] = {DRM_FORMAT_R8, rg_fourcc, 0, 0};
            return format;
        }

        case QVideoFrameFormat::Format_P010:
        case QVideoFrameFormat::Format_P016:
        {
            static constexpr quint32 format[] = {DRM_FORMAT_R16, rg16_fourcc, 0, 0};
            return format;
        }
    }
    return nullptr;
}

QString eglErrorString(EGLint error)
{
    switch (error)
    {
        case EGL_SUCCESS: return "No error";
        case EGL_NOT_INITIALIZED: return "EGL not initialized or failed to initialize";
        case EGL_BAD_ACCESS: return "Resource inaccessible";
        case EGL_BAD_ALLOC: return "Cannot allocate resources";
        case EGL_BAD_ATTRIBUTE: return "Unrecognized attribute or attribute value";
        case EGL_BAD_CONTEXT: return "Invalid EGL context";
        case EGL_BAD_CONFIG: return "Invalid EGL frame buffer configuration";
        case EGL_BAD_CURRENT_SURFACE: return "Current surface is no longer valid";
        case EGL_BAD_DISPLAY: return "Invalid EGL display";
        case EGL_BAD_SURFACE: return "Invalid surface";
        case EGL_BAD_MATCH: return "Inconsistent arguments";
        case EGL_BAD_PARAMETER: return "Invalid argument";
        case EGL_BAD_NATIVE_PIXMAP: return "Invalid native pixmap";
        case EGL_BAD_NATIVE_WINDOW: return "Invalid native window";
        case EGL_CONTEXT_LOST: return "Context lost";
    }
    return QString("Unknown error 0x%1").arg((int) error, 0, 16);
}

QString glErrorString(GLenum error)
{
    switch (error)
    {
        case GL_NO_ERROR:
            return "No error has been recorded.";
        case GL_INVALID_ENUM:
            return "An unacceptable value is specified for an enumerated argument.";

        case GL_INVALID_VALUE:
            return "A numeric argument is out of range.";

        case GL_INVALID_OPERATION:
            return "The specified operation is not allowed in the current state.";

        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "The framebuffer object is not complete.";

        case GL_OUT_OF_MEMORY:
            return "There is not enough memory left to execute the command.";

        case GL_STACK_UNDERFLOW:
            return "An attempt has been made to perform an operation that would cause an internal"
                " stack to underflow.";

        case GL_STACK_OVERFLOW:
            return "An attempt has been made to perform an operation that would cause an internal"
                " stack to overflow.";

        default:
            return QString("Unknown error 0x%1").arg((int) error, 0, 16);
    }
}

class VideoFrameTextures: public QVideoFrameTextures
{
public:
    VideoFrameTextures(int nPlanes): m_nPlanes(nPlanes)
    {
    }

    virtual ~VideoFrameTextures() override
    {
        auto rhi = m_textures[0] ? m_textures[0]->rhi() : nullptr;
        if (!rhi)
            return;

        auto nativeHandles = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles());
        auto glContext = nativeHandles->context;
        if (!glContext)
            return;

        rhi->makeThreadLocalNativeContextCurrent();
        QOpenGLFunctions functions(glContext);
        functions.glDeleteTextures(m_nPlanes, &m_handles[0]);
    }

    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane >= m_textures.size())
            return nullptr;

        return m_textures[plane].get();
    }

    int m_nPlanes = 0;
    std::array<GLuint, 4> m_handles;
    std::array<std::unique_ptr<QRhiTexture>, 4> m_textures;
};

QString drmName(uint32_t fourcc)
{
    QString result;
    for (int i = 0; i < 4; ++i)
    {
        result.append(QChar(fourcc & 0xFF));
        fourcc >>= 8;
    }
    return result;
}

class VaapiMemoryBuffer: public QAbstractVideoBuffer
{
    using base_type = QAbstractVideoBuffer;

public:
    VaapiMemoryBuffer(const AVFrame* frame):
        base_type(QVideoFrame::NoHandle),
        m_frame(frame)
    {
    }

    ~VaapiMemoryBuffer()
    {
    }

    virtual MapData map(QVideoFrame::MapMode) override
    {
        return {};
    }

    virtual void unmap() override
    {
    }

    virtual std::unique_ptr<QVideoFrameTextures> mapTextures(QRhi* rhi) override
    {
        if (!m_frame->hw_frames_ctx)
            return {};

        auto converter = getTextureConverterForRhi(rhi);

        auto fCtx = (AVHWFramesContext*) m_frame->hw_frames_ctx->data;

        AVHWDeviceContext* ctx = fCtx->device_ctx;
        if (!ctx)
            return nullptr;

        auto vaCtx = (AVVAAPIDeviceContext *)ctx->hwctx;
        VADisplay vaDisplay = vaCtx->display;
        if (!vaDisplay)
        {
            NX_WARNING(this, "no VADisplay for mapping textures");
            return {};
        }

        VASurfaceID vaSurface = (uintptr_t) m_frame->data[3];

        if (!converter->isEgl())
            return {};

        bool useLayers = true; // separate layers required for nvidia-vaapi-driver
        const uint32_t kLayersFlag = useLayers
            ? VA_EXPORT_SURFACE_SEPARATE_LAYERS
            : VA_EXPORT_SURFACE_COMPOSED_LAYERS;

        VADRMPRIMESurfaceDescriptor prime;
        VAStatus status = vaExportSurfaceHandle(
            vaDisplay,
            vaSurface,
            VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
            VA_EXPORT_SURFACE_READ_ONLY | kLayersFlag,
            &prime);

        if (status != VA_STATUS_SUCCESS)
        {
            NX_WARNING(this, "vaExportSurfaceHandle() failed %1 %2", status, vaErrorStr(status));
            return {};
        }

        nx::utils::ScopeGuard closeObjectsGuard(
            [&prime]()
            {
                for (uint32_t i = 0;  i < prime.num_objects;  ++i)
                    close(prime.objects[i].fd);
            });

        status = vaSyncSurface(vaDisplay, vaSurface);

        if (status != VA_STATUS_SUCCESS)
        {
            NX_WARNING(this, "vaSyncSurface() failed %1 %2", status, vaErrorStr(status));
            return {};
        }

        auto qtFormat = toQtPixelFormat(m_frame);
        auto drm_formats = fourccFromPixelFormat(qtFormat);

        auto *desc = QVideoTextureHelper::textureDescription(qtFormat);
        int nPlanes = 0;
        for (; nPlanes < 5; ++nPlanes)
        {
            if (drm_formats[nPlanes] == 0)
                break;
        }
        NX_ASSERT(nPlanes == desc->nplanes);
        nPlanes = desc->nplanes;

        rhi->makeThreadLocalNativeContextCurrent();

        QOpenGLFunctions functions(converter->m_glContext);

        EGLImage images[4];
        GLuint glTextures[4] = {};
        functions.glGenTextures(nPlanes, glTextures);
        GLenum glError = functions.glGetError();
        if (glError != GL_NO_ERROR)
            NX_WARNING(this, "glGenTextures() failed %1 %2", glError, glErrorString(glError));

        const auto drm_format_modifier = converter->useModifiers()
            ? prime.objects[0].drm_format_modifier
            : DRM_FORMAT_MOD_INVALID;
        const EGLAttrib modifier = drm_format_modifier == DRM_FORMAT_MOD_INVALID
            ? EGL_NONE
            : EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;

        for (int i = 0;  i < nPlanes;  ++i)
        {
            int layer = 0;
            int plane = i;
            if (useLayers)
            {
                layer = i;
                plane = 0;
                if (prime.layers[i].drm_format != drm_formats[i])
                {
                    NX_WARNING(this,
                        "Expected DRM format check failed, expected '%1' got '%2'",
                        drmName(drm_formats[i]),
                        drmName(prime.layers[i].drm_format));
                }
            }

            const EGLAttrib imgAttr[] = {
                EGL_LINUX_DRM_FOURCC_EXT,           (EGLint)prime.layers[i].drm_format,
                EGL_WIDTH,                          desc->widthForPlane(m_frame->width, i),
                EGL_HEIGHT,                         desc->heightForPlane(m_frame->height, i),
                EGL_DMA_BUF_PLANE0_FD_EXT,          prime.objects[prime.layers[layer].object_index[plane]].fd,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT,      (EGLint)prime.layers[layer].offset[plane],
                EGL_DMA_BUF_PLANE0_PITCH_EXT,       (EGLint)prime.layers[layer].pitch[plane],
                modifier,                           static_cast<EGLAttrib>(drm_format_modifier & 0xfffffffful),
                EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLAttrib>(drm_format_modifier >> 32),
                EGL_NONE
            };
            images[i] = eglCreateImage(
                converter->m_eglDisplay,
                EGL_NO_CONTEXT,
                EGL_LINUX_DMA_BUF_EXT,
                /* buffer */ nullptr,
                imgAttr);

            if (!images[i])
            {
                const EGLint error = eglGetError();
                if (error == EGL_BAD_MATCH)
                {
                    NX_WARNING(this,
                        "eglCreateImage failed for plane %1 with error code EGL_BAD_MATCH."
                        " VAAPI driver: '%2' EGL vendor: '%3'. Disabling hardware acceleration.",
                        i,
                        vaQueryVendorString(vaDisplay),
                        eglQueryString(converter->m_eglDisplay, EGL_VENDOR));

                    converter->m_rhi = nullptr;
                    return nullptr;
                }
                if (error)
                {
                    NX_WARNING(this,
                        "eglCreateImage failed for plane %1 with error %2 %3",
                        i,
                        error,
                        eglErrorString(error));
                    return nullptr;
                }
            }

            functions.glActiveTexture(GL_TEXTURE0 + i);
            glError = functions.glGetError();
            if (glError != GL_NO_ERROR)
                NX_WARNING(this, "glActiveTexture() failed %1 %2", glError, glErrorString(glError));

            functions.glBindTexture(GL_TEXTURE_2D, glTextures[i]);

            glError = functions.glGetError();
            if (glError != GL_NO_ERROR)
                NX_WARNING(this, "glBindTexture() failed %1 %2", glError, glErrorString(glError));

            auto eglImageTargetTexture2D =
                (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)converter->m_eglImageTargetTexture2D;
            eglImageTargetTexture2D(GL_TEXTURE_2D, images[i]);

            glError = functions.glGetError();
            if (glError != GL_NO_ERROR)
            {
                NX_WARNING(this,
                    "glEGLImageTargetTexture2DOES() failed %1 %2", glError, glErrorString(glError));
            }
        }

        for (int i = 0;  i < nPlanes;  ++i)
        {
            functions.glActiveTexture(GL_TEXTURE0 + i);
            functions.glBindTexture(GL_TEXTURE_2D, 0);
            eglDestroyImage(converter->m_eglDisplay, images[i]);
        }

        auto textures = std::make_unique<VideoFrameTextures>(nPlanes);

        QVideoFrameFormat format(QSize(m_frame->width, m_frame->height), qtFormat);

        for (int plane = 0; plane < 4; ++plane)
        {
            textures->m_handles[plane] = glTextures[plane];
            textures->m_textures[plane] = createTextureFromHandle(
                format, rhi, plane, textures->m_handles[plane]);
        }

        return textures;
    }

private:
    const AVFrame* m_frame = nullptr;
};

} // namespace

class VaApiVideoApiEntry: public VideoApiRegistry::Entry
{
public:
    VaApiVideoApiEntry()
    {
        VideoApiRegistry::instance()->add(AV_HWDEVICE_TYPE_VAAPI, this);
    }

    virtual AVHWDeviceType deviceType() const override
    {
        return AV_HWDEVICE_TYPE_VAAPI;
    }

    virtual nx::media::VideoFramePtr makeFrame(const AVFrame* frame) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_VAAPI))
            return {};

        const QSize frameSize(frame->width, frame->height);
        const int64_t timestampUs = frame->pkt_dts;

        QAbstractVideoBuffer* buffer = new VaapiMemoryBuffer(frame);

        const auto qtPixelFormat = toQtPixelFormat(frame);

        auto result = std::make_shared<VideoFrame>(
            buffer,
            QVideoFrameFormat{frameSize, qtPixelFormat});
        result->setStartTime(timestampUs);

        return result;
    }

    virtual std::unique_ptr<nx::media::ffmpeg::AvOptions> options(QRhi* rhi) const override
    {
        if (!NX_ASSERT(rhi && rhi->backend() == QRhi::Implementation::OpenGLES2))
            return {};

        auto options = std::make_unique<nx::media::ffmpeg::AvOptions>();
        options->set("connection_type", "x11");
        return options;
    }
};

VaApiVideoApiEntry g_vaApiVideoApiEntry;

} // namespace nx::media

#endif // defined(__x86_64__)
