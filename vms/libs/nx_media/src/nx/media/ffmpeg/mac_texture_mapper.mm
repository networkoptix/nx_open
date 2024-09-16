// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_texture_mapper.h"

#include <QtMultimedia/private/qabstractvideobuffer_p.h>
#include <QtMultimedia/private/qvideotexturehelper_p.h>

#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CMFormatDescription.h>

#include <nx/utils/log/log.h>

#include "texture_helper.h"

namespace {

static constexpr auto kMaxPlanes = 4;

static MTLPixelFormat rhiTextureFormatToMetalFormat(QRhiTexture::Format format)
{
    switch (format)
    {
        case QRhiTexture::RGBA8:
            return MTLPixelFormatRGBA8Unorm;
        case QRhiTexture::BGRA8:
            return MTLPixelFormatBGRA8Unorm;
        case QRhiTexture::R8:
            return MTLPixelFormatR8Unorm;
        case QRhiTexture::RG8:
            return MTLPixelFormatRG8Unorm;
        case QRhiTexture::R16:
            return MTLPixelFormatR16Unorm;
        case QRhiTexture::RG16:
            return MTLPixelFormatRG16Unorm;
        case QRhiTexture::RGBA16F:
            return MTLPixelFormatRGBA16Float;
        case QRhiTexture::RGBA32F:
            return MTLPixelFormatRGBA32Float;
        case QRhiTexture::R16F:
            return MTLPixelFormatR16Float;
        case QRhiTexture::R32F:
            return MTLPixelFormatR32Float;
        case QRhiTexture::UnknownFormat:
        default:
            return MTLPixelFormatInvalid;
    }
}

using PixelFormat = QVideoFrameFormat::PixelFormat;
using ColorRange = QVideoFrameFormat::ColorRange;

using CvPixelFormat = unsigned;

constexpr std::tuple<CvPixelFormat, PixelFormat, ColorRange> PixelFormatMap[] = {
    {kCVPixelFormatType_32ARGB, PixelFormat::Format_ARGB8888, ColorRange::ColorRange_Unknown},
    {kCVPixelFormatType_32BGRA, PixelFormat::Format_BGRA8888, ColorRange::ColorRange_Unknown},
    {kCVPixelFormatType_420YpCbCr8Planar, PixelFormat::Format_YUV420P, ColorRange::ColorRange_Unknown},
    {kCVPixelFormatType_420YpCbCr8PlanarFullRange, PixelFormat::Format_YUV420P, ColorRange::ColorRange_Full},
    {kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, PixelFormat::Format_NV12, ColorRange::ColorRange_Video},
    {kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, PixelFormat::Format_NV12, ColorRange::ColorRange_Full},
    {kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, PixelFormat::Format_P010, ColorRange::ColorRange_Video},
    {kCVPixelFormatType_420YpCbCr10BiPlanarFullRange, PixelFormat::Format_P010, ColorRange::ColorRange_Full},
    {kCVPixelFormatType_422YpCbCr8, PixelFormat::Format_UYVY, ColorRange::ColorRange_Video},
    {kCVPixelFormatType_422YpCbCr8_yuvs, PixelFormat::Format_YUYV, ColorRange::ColorRange_Video},
    {kCVPixelFormatType_OneComponent8, PixelFormat::Format_Y8, ColorRange::ColorRange_Unknown},
    {kCVPixelFormatType_OneComponent16, PixelFormat::Format_Y16, ColorRange::ColorRange_Unknown},

    // Matching kCMVideoCodecType_JPEG_OpenDML to ColorRange_Full used to distinguish between
    // kCMVideoCodecType_JPEG and kCMVideoCodecType_JPEG_OpenDML.
    {kCMVideoCodecType_JPEG, PixelFormat::Format_Jpeg, ColorRange::ColorRange_Unknown},
    {kCMVideoCodecType_JPEG_OpenDML, PixelFormat::Format_Jpeg, ColorRange::ColorRange_Full}
};

template<typename Type, typename... Args>
Type findInPixelFormatMap(Type defaultValue, Args... args)
{
    auto checkElement = [&](const auto &element) {
        return ((args == std::get<Args>(element)) && ...);
    };

    auto found = std::find_if(std::begin(PixelFormatMap), std::end(PixelFormatMap), checkElement);
    return found == std::end(PixelFormatMap) ? defaultValue : std::get<Type>(*found);
}

PixelFormat fromCVPixelFormat(CvPixelFormat cvPixelFormat)
{
    return findInPixelFormatMap(PixelFormat::Format_Invalid, cvPixelFormat);
}

ColorRange colorRangeForCVPixelFormat(CvPixelFormat cvPixelFormat)
{
    return findInPixelFormatMap(ColorRange::ColorRange_Unknown, cvPixelFormat);
}

QVideoFrameFormat videoFormatForImageBuffer(CVImageBufferRef buffer, bool openGL = false)
{
    auto cvPixelFormat = CVPixelBufferGetPixelFormatType(buffer);
    auto pixelFormat = fromCVPixelFormat(cvPixelFormat);
    if (openGL)
    {
        if (cvPixelFormat == kCVPixelFormatType_32BGRA)
        {
            pixelFormat = QVideoFrameFormat::Format_SamplerRect;
        }
        else
        {
            NX_WARNING(
                NX_SCOPE_TAG,
                "Accelerated macOS OpenGL video supports BGRA only, got CV pixel format %1",
                cvPixelFormat);
        }
    }

    size_t width = CVPixelBufferGetWidth(buffer);
    size_t height = CVPixelBufferGetHeight(buffer);

    QVideoFrameFormat format(QSize(width, height), pixelFormat);

    auto colorSpace = QVideoFrameFormat::ColorSpace_Undefined;
    auto colorTransfer = QVideoFrameFormat::ColorTransfer_Unknown;

    if (CFStringRef cSpace = reinterpret_cast<CFStringRef>(
        CVBufferGetAttachment(buffer, kCVImageBufferYCbCrMatrixKey, nullptr)))
    {
        if (CFEqual(cSpace, kCVImageBufferYCbCrMatrix_ITU_R_709_2))
        {
            colorSpace = QVideoFrameFormat::ColorSpace_BT709;
        }
        else if (CFEqual(cSpace, kCVImageBufferYCbCrMatrix_ITU_R_601_4)
            || CFEqual(cSpace, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995))
        {
            colorSpace = QVideoFrameFormat::ColorSpace_BT601;
        }
        else if (@available(macOS 10.11, iOS 9.0, *))
        {
            if (CFEqual(cSpace, kCVImageBufferYCbCrMatrix_ITU_R_2020))
                colorSpace = QVideoFrameFormat::ColorSpace_BT2020;
        }
    }

    if (CFStringRef cTransfer = reinterpret_cast<CFStringRef>(
        CVBufferGetAttachment(buffer, kCVImageBufferTransferFunctionKey, nullptr)))
    {

        if (CFEqual(cTransfer, kCVImageBufferTransferFunction_ITU_R_709_2))
        {
            colorTransfer = QVideoFrameFormat::ColorTransfer_BT709;
        }
        else if (CFEqual(cTransfer, kCVImageBufferTransferFunction_SMPTE_240M_1995))
        {
            colorTransfer = QVideoFrameFormat::ColorTransfer_BT601;
        }
        else if (CFEqual(cTransfer, kCVImageBufferTransferFunction_sRGB))
        {
            colorTransfer = QVideoFrameFormat::ColorTransfer_Gamma22;
        }
        else if (CFEqual(cTransfer, kCVImageBufferTransferFunction_UseGamma))
        {
            auto gamma = reinterpret_cast<CFNumberRef>(
                        CVBufferGetAttachment(buffer, kCVImageBufferGammaLevelKey, nullptr));
            double g = 0.0;
            CFNumberGetValue(gamma, kCFNumberFloat32Type, &g);
            // These are best fit values given what we have in Qt enum.
            if (g < 0.8)
                colorTransfer = QVideoFrameFormat::ColorTransfer_Unknown;
            else if (g < 1.5)
                colorTransfer = QVideoFrameFormat::ColorTransfer_Linear;
            else if (g < 2.1)
                colorTransfer = QVideoFrameFormat::ColorTransfer_BT709;
            else if (g < 2.5)
                colorTransfer = QVideoFrameFormat::ColorTransfer_Gamma22;
            else if (g < 3.2)
                colorTransfer = QVideoFrameFormat::ColorTransfer_Gamma28;
        }
        if (@available(macOS 10.12, iOS 11.0, *))
        {
            if (CFEqual(cTransfer, kCVImageBufferTransferFunction_ITU_R_2020))
                colorTransfer = QVideoFrameFormat::ColorTransfer_BT709;
        }
        if (@available(macOS 10.12, iOS 11.0, *))
        {
            if (CFEqual(cTransfer, kCVImageBufferTransferFunction_ITU_R_2100_HLG))
                colorTransfer = QVideoFrameFormat::ColorTransfer_STD_B67;
            else if (CFEqual(cTransfer, kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ))
                colorTransfer = QVideoFrameFormat::ColorTransfer_ST2084;
        }
    }

    format.setColorRange(colorRangeForCVPixelFormat(cvPixelFormat));
    format.setColorSpace(colorSpace);
    format.setColorTransfer(colorTransfer);
    return format;
}

} // namespace

CVMetalTextureCacheRef mac_getCacheForRhi(QRhi* rhi)
{
    static std::mutex mutex;
    static QHash<QRhi*, CVMetalTextureCacheRef> allRhis;

    std::lock_guard lock(mutex);

    if (CVMetalTextureCacheRef cache = allRhis.value(rhi, nullptr))
        return cache;

    if (rhi->backend() != QRhi::Metal)
        return nullptr;

    CVMetalTextureCacheRef cache = nullptr;

    const auto metal = static_cast<const QRhiMetalNativeHandles*>(rhi->nativeHandles());

    const auto ret = CVMetalTextureCacheCreate(
        kCFAllocatorDefault,
        /* cacheAttributes */ nil,
        (id<MTLDevice>) metal->dev,
        /* textureAttributes */ nil,
        &cache);

    if (ret != kCVReturnSuccess)
    {
        NX_WARNING(NX_SCOPE_TAG, "Metal texture cache creation failed: %1", ret);
        return nullptr;
    }

    rhi->addCleanupCallback(
        [](QRhi* rhi)
        {
            std::lock_guard lock(mutex);
            if (CVMetalTextureCacheRef cache = allRhis.value(rhi, nullptr))
                CFRelease(cache);
            allRhis.remove(rhi);
        });

    allRhis[rhi] = cache;

    return cache;
}

class MacTextureSet: public MacTextureMapper
{
public:
    ~MacTextureSet() override;
    void createTextures(CVImageBufferRef buffer, QRhi* rhi);

    bool isNull() const { return m_handles[0] == 0; }

    std::unique_ptr<QVideoFrameTextures> mapTextures(QRhi* rhi) override;

private:
    void clear();

private:
    QVideoFrameFormat m_format;
    CVMetalTextureRef m_cvMetalTexture[kMaxPlanes] = {};
    quint64 m_handles[kMaxPlanes] = {};
    CVImageBufferRef m_buffer = nullptr;
};

std::unique_ptr<MacTextureMapper> MacTextureMapper::create(CVImageBufferRef buffer, QRhi* rhi)
{
    auto mapper = std::make_unique<MacTextureSet>();
    mapper->createTextures(buffer, rhi);
    return mapper;
}

void MacTextureSet::createTextures(CVImageBufferRef buffer, QRhi* rhi)
{
    if (rhi->backend() != QRhi::Metal)
        return;

    auto metalCache = mac_getCacheForRhi(rhi);

    clear();

    m_buffer = buffer;
    CVPixelBufferRetain(buffer);

    m_format = videoFormatForImageBuffer(buffer);

    auto* textureDescription = QVideoTextureHelper::textureDescription(m_format.pixelFormat());

    const auto bufferPlanes = CVPixelBufferGetPlaneCount(buffer);

    const size_t width = CVPixelBufferGetWidth(buffer);
    const size_t height = CVPixelBufferGetHeight(buffer);

    for (size_t plane = 0; plane < bufferPlanes; ++plane)
    {
        const size_t planeWidth = textureDescription->widthForPlane(width, plane);
        const size_t planeHeight = textureDescription->heightForPlane(height, plane);

        const auto ret = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            metalCache,
            buffer, nil,
            rhiTextureFormatToMetalFormat(textureDescription->textureFormat[plane]),
            planeWidth, planeHeight,
            plane,
            &m_cvMetalTexture[plane]);

        if (ret != kCVReturnSuccess)
            NX_WARNING(this, "Texture creation failed: %1", ret);

        m_handles[plane] = (qint64) CVMetalTextureGetTexture(m_cvMetalTexture[plane]);
    }
}

void MacTextureSet::clear()
{
    for (int i = 0; i < kMaxPlanes; ++i)
    {
        if (m_cvMetalTexture[i])
        {
            CFRelease(m_cvMetalTexture[i]);
            m_cvMetalTexture[i] = nullptr;
        }
    }
    if (m_buffer)
    {
        CVPixelBufferRelease(m_buffer);
        m_buffer = nullptr;
    }
    memset(m_handles, 0, sizeof(m_handles));
}

MacTextureSet::~MacTextureSet()
{
    clear();
}

class VideoFrameTextures: public QVideoFrameTextures
{
public:
    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane >= m_textures.size())
            return nullptr;

        return m_textures[plane].get();
    }

    std::array<std::unique_ptr<QRhiTexture>, kMaxPlanes> m_textures;
};

std::unique_ptr<QVideoFrameTextures> MacTextureSet::mapTextures(QRhi* rhi)
{
    if (!m_handles[0])
        return {};

    auto textures = std::make_unique<VideoFrameTextures>();

    for (size_t plane = 0; plane < textures->m_textures.size(); ++plane)
    {
        textures->m_textures[plane] = nx::media::createTextureFromHandle(
            m_format, rhi, plane, m_handles[plane]);
    }

    return textures;
}
