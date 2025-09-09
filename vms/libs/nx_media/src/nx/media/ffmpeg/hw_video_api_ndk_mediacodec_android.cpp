// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <android/native_window_jni.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <QtCore/QJniEnvironment>
#include <QtGui/QGuiApplication>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QVulkanFunctions>
#include <QtGui/QVulkanInstance>
#include <QtGui/private/qvulkandefaultinstance_p.h>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/rhi/qrhi.h>
#include <QtMultimedia/QVideoFrameFormat>
#include <QtMultimedia/private/qhwvideobuffer_p.h>

#include <nx/media/avframe_memory_buffer.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>
#include <nx/media/utils.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

extern "C" {
#include <libavcodec/jni.h>
#include <libavcodec/mediacodec.h>
} // extern "C"

#include "gl_common.h"
#include "hw_video_api.h"
#include "texture_helper.h"

namespace nx::media {

namespace {

struct ImportedImage
{
    AHardwareBuffer* ahb = nullptr; //< Not owned, AImage holds it.
    VkDevice device = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t externalFormat = 0;

    ImportedImage() = default;

    ImportedImage(const ImportedImage&) = delete;
    ImportedImage& operator=(const ImportedImage&) = delete;

    ImportedImage(ImportedImage&& other) noexcept
        :
        ahb(other.ahb),
        device(std::exchange(other.device, (VkDevice) VK_NULL_HANDLE)),
        image(std::exchange(other.image, (VkImage) VK_NULL_HANDLE)),
        memory(std::exchange(other.memory, (VkDeviceMemory) VK_NULL_HANDLE)),
        width(other.width),
        height(other.height),
        externalFormat(other.externalFormat)
    {
    }

    ImportedImage& operator=(ImportedImage&& other) noexcept
    {
        if (this == &other)
            return *this;

        clear();

        ahb = other.ahb;
        device = std::exchange(other.device, (VkDevice) VK_NULL_HANDLE);
        image = std::exchange(other.image, (VkImage) VK_NULL_HANDLE);
        memory = std::exchange(other.memory, (VkDeviceMemory) VK_NULL_HANDLE);
        width = other.width;
        height = other.height;
        externalFormat = other.externalFormat;

        return *this;
    }

    void clear()
    {
        QVulkanInstance* inst = QVulkanDefaultInstance::instance();
        if (!NX_ASSERT(inst))
            return;

        if (!device)
            return;

        QVulkanDeviceFunctions* f = inst->deviceFunctions(device);
        if (image)
            f->vkDestroyImage(device, image, nullptr);
        if (memory)
            f->vkFreeMemory(device, memory, nullptr);
    }

    ~ImportedImage()
    {
        clear();
    }
};

class ScopedEGLImage
{
public:
    ScopedEGLImage() = default;

    ScopedEGLImage(EGLImageKHR image, EGLDisplay display, PFNEGLDESTROYIMAGEKHRPROC destroyFunc)
        :
        m_image(image),
        m_display(display),
        m_eglDestroyImage(destroyFunc)
    {
    }

    ScopedEGLImage(ScopedEGLImage&& other) noexcept
        :
        m_image(std::exchange(other.m_image, EGL_NO_IMAGE)),
        m_display(std::exchange(other.m_display, EGL_NO_DISPLAY)),
        m_eglDestroyImage(std::exchange(other.m_eglDestroyImage, nullptr))
    {
    }

    ScopedEGLImage& operator=(ScopedEGLImage&& other) noexcept
    {
        if (this == &other)
            return *this;

        clear();
        m_image = std::exchange(other.m_image, EGL_NO_IMAGE);
        m_display = std::exchange(other.m_display, EGL_NO_DISPLAY);
        m_eglDestroyImage = std::exchange(other.m_eglDestroyImage, nullptr);

        return *this;
    }

    ScopedEGLImage(const ScopedEGLImage&) = delete;
    ScopedEGLImage& operator=(const ScopedEGLImage&) = delete;

    ~ScopedEGLImage()
    {
        clear();
    }

    EGLImageKHR value() const noexcept { return m_image; }

private:
    void clear()
    {
        if (m_image && m_eglDestroyImage)
            m_eglDestroyImage(m_display, m_image);
    }

private:
    EGLImageKHR m_image = EGL_NO_IMAGE;
    EGLDisplay m_display = EGL_NO_DISPLAY;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImage = nullptr;
};

class VkVideoFrameTextures: public QVideoFrameTextures
{
public:
    VkVideoFrameTextures()
    {
    }

    virtual ~VkVideoFrameTextures() override
    {
    }

    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane != 0)
            return nullptr;

        return m_texture.get();
    }

    ImportedImage m_image;
    std::unique_ptr<QRhiTexture> m_texture;
};

QString amediaErrorString(media_status_t status)
{
    switch (status)
    {
        case AMEDIA_OK: return "AMEDIA_OK";
        case AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE:
            return "AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE";
        case AMEDIACODEC_ERROR_RECLAIMED: return "AMEDIACODEC_ERROR_RECLAIMED";
        case AMEDIA_ERROR_MALFORMED: return "AMEDIA_ERROR_MALFORMED";
        case AMEDIA_ERROR_UNSUPPORTED: return "AMEDIA_ERROR_UNSUPPORTED";
        case AMEDIA_ERROR_INVALID_OBJECT: return "AMEDIA_ERROR_INVALID_OBJECT";
        case AMEDIA_ERROR_INVALID_PARAMETER: return "AMEDIA_ERROR_INVALID_PARAMETER";
        case AMEDIA_ERROR_INVALID_OPERATION: return "AMEDIA_ERROR_INVALID_OPERATION";
        case AMEDIA_ERROR_END_OF_STREAM: return "AMEDIA_ERROR_END_OF_STREAM";
        case AMEDIA_ERROR_IO: return "AMEDIA_ERROR_IO";
        case AMEDIA_ERROR_WOULD_BLOCK: return "AMEDIA_ERROR_WOULD_BLOCK";
        case AMEDIA_DRM_NOT_PROVISIONED: return "AMEDIA_DRM_NOT_PROVISIONED";
        case AMEDIA_DRM_RESOURCE_BUSY: return "AMEDIA_DRM_RESOURCE_BUSY";
        case AMEDIA_DRM_DEVICE_REVOKED: return "AMEDIA_DRM_DEVICE_REVOKED";
        case AMEDIA_DRM_SHORT_BUFFER: return "AMEDIA_DRM_SHORT_BUFFER";
        case AMEDIA_DRM_SESSION_NOT_OPENED: return "AMEDIA_DRM_SESSION_NOT_OPENED";
        case AMEDIA_DRM_TAMPER_DETECTED: return "AMEDIA_DRM_TAMPER_DETECTED";
        case AMEDIA_DRM_VERIFY_FAILED: return "AMEDIA_DRM_VERIFY_FAILED";
        case AMEDIA_DRM_NEED_KEY: return "AMEDIA_DRM_NEED_KEY";
        case AMEDIA_DRM_LICENSE_EXPIRED: return "AMEDIA_DRM_LICENSE_EXPIRED";
        case AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE: return "AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE";
        case AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED: return "AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED";
        case AMEDIA_IMGREADER_CANNOT_LOCK_IMAGE: return "AMEDIA_IMGREADER_CANNOT_LOCK_IMAGE";
        case AMEDIA_IMGREADER_CANNOT_UNLOCK_IMAGE: return "AMEDIA_IMGREADER_CANNOT_UNLOCK_IMAGE";
        case AMEDIA_IMGREADER_IMAGE_NOT_LOCKED: return "AMEDIA_IMGREADER_IMAGE_NOT_LOCKED";
        case AMEDIA_ERROR_UNKNOWN: return "AMEDIA_ERROR_UNKNOWN";
        default:
            return QString("Unknown error 0x%1").arg((int) status, 0, 16);
    }
}

struct AImageReaderDeleter
{
    void operator()(AImageReader* reader) const
    {
        if (reader)
            AImageReader_delete(reader);
    }
};

struct AImageDeleter
{
    void operator()(AImage* reader) const
    {
        if (reader)
            AImage_delete(reader);
    }
};

class DecoderData: public VideoApiDecoderData
{
    using base_type = VideoApiDecoderData;

public:
    DecoderData(QRhi* rhi, const QSize& frameSize):
        base_type(rhi),
        frameSize(frameSize)
    {
        static const int32_t kMaxImages = 10;

        {
            AImageReader* reader = nullptr;
            media_status_t status = AImageReader_newWithUsage(
                frameSize.width(),
                frameSize.height(),
                AIMAGE_FORMAT_PRIVATE,
                AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
                kMaxImages,
                &reader
            );
            if (status != AMEDIA_OK)
                NX_ERROR(this, "AImageReader_newWithUsage failed: %1", amediaErrorString(status));

            imageReader.reset(reader);
        }

        static std::once_flag initFfmpegJvmFlag;
        std::call_once(initFfmpegJvmFlag,
            []()
            {
                av_jni_set_java_vm(QJniEnvironment::javaVM(), nullptr);
            });

        switch (rhi->backend())
        {
            case QRhi::OpenGLES2:
                initOpenGL();
                break;
            case QRhi::Vulkan:
                initVulkan();
                break;
            default:
                NX_ASSERT(false, "Unsupported backend %1", rhi->backend());
        }
    }

    void initOpenGL()
    {
        auto nativeHandles = static_cast<const QRhiGles2NativeHandles*>(rhi()->nativeHandles());
        QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
        m_eglDisplay = pni->nativeResourceForContext(
            QByteArrayLiteral("egldisplay"),
            nativeHandles->context);

        if (!m_eglDisplay)
        {
            NX_WARNING(this, "Cannot get EGLDisplay from OpenGL ES context, trying default");
            m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (!m_eglDisplay)
            {
                NX_ERROR(this, "Cannot get default EGLDisplay");
                return;
            }
        }

        if (!nativeHandles->context->hasExtension("GL_OES_EGL_image_external"))
        {
            NX_ERROR(this, "GL_OES_EGL_image_external is required");
            return;
        }

        m_eglGetNativeClientBuffer = (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC)
            eglGetProcAddress("eglGetNativeClientBufferANDROID");
        if (!m_eglGetNativeClientBuffer)
        {
            NX_ERROR(this, "Failed to get eglGetNativeClientBufferANDROID address");
            return;
        }

        m_eglCreateImage = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
        if (!m_eglCreateImage)
        {
            NX_ERROR(this, "Failed to get eglCreateImageKHR address");
            return;
        }

        m_eglDestroyImage = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
        if (!m_eglDestroyImage)
        {
            NX_ERROR(this, "Failed to get eglDestroyImageKHR address");
            return;
        }

        m_eglImageTargetTexture2D = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
            eglGetProcAddress("glEGLImageTargetTexture2DOES");
        if (!m_eglImageTargetTexture2D)
        {
            NX_ERROR(this, "Failed to get glEGLImageTargetTexture2DOES address");
            return;
        }
    }

    void initVulkan()
    {
        QVulkanInstance* inst = QVulkanDefaultInstance::instance();
        if (!NX_ASSERT(inst))
            return;

        m_vkGetAndroidHardwareBufferProperties =
        (PFN_vkGetAndroidHardwareBufferPropertiesANDROID) inst->getInstanceProcAddr(
            "vkGetAndroidHardwareBufferPropertiesANDROID");

        if (!m_vkGetAndroidHardwareBufferProperties)
            NX_ERROR(this, "Failed to get vkGetAndroidHardwareBufferPropertiesANDROID address");
    }

    bool isInitialized() const
    {
        switch (rhi()->backend())
        {
            case QRhi::OpenGLES2:
                return (bool) m_eglImageTargetTexture2D;
            case QRhi::Vulkan:
                return (bool) m_vkGetAndroidHardwareBufferProperties;
            default:
                return false;
        }
    }

    ~DecoderData() override
    {
    }

    jobject getSurface() const
    {
        ANativeWindow* window = nullptr;
        media_status_t status = AImageReader_getWindow(imageReader.get(), &window);
        if (status != AMEDIA_OK)
        {
            NX_ERROR(this, "AImageReader_getWindow failed: %1", amediaErrorString(status));
            return nullptr;
        }

        QJniEnvironment env;
        return ANativeWindow_toSurface(env.jniEnv(), window);
    }

    std::shared_ptr<AImage> imageFromFrame(const AVFrame* frame);

    bool importAHBToVkImage(VkDevice device, AHardwareBuffer* ahb, ImportedImage* out);
    std::tuple<GLuint, ScopedEGLImage> importAHBtoGLTexture(QRhi* rhi, AHardwareBuffer* ahb);

    std::unique_ptr<AImageReader, AImageReaderDeleter> imageReader;

    QSize frameSize;
    EGLDisplay m_eglDisplay = nullptr;
    PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC m_eglGetNativeClientBuffer = nullptr;
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImage = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImage = nullptr;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_eglImageTargetTexture2D = nullptr;
    PFN_vkGetAndroidHardwareBufferPropertiesANDROID m_vkGetAndroidHardwareBufferProperties =
        nullptr;
};

std::shared_ptr<AImage> DecoderData::imageFromFrame(const AVFrame* frame)
{
    if (!frame->data[3])
    {
        NX_WARNING(this, "No MediaCodec buffer found in output frame");
        return {};
    }

    if (av_mediacodec_release_buffer((AVMediaCodecBuffer*) frame->data[3], /*render*/ 1) < 0)
    {
        NX_WARNING(this, "Failed to render MediaCodec buffer");
        return {};
    }

    AImage* imagePtr = nullptr;
    media_status_t status = AImageReader_acquireLatestImage(
        imageReader.get(),
        &imagePtr);

    if (status != AMEDIA_OK)
    {
        NX_ERROR(this, "AImageReader_acquireLatestImage failed: %1", amediaErrorString(status));
        return {};
    }

    std::shared_ptr<AImage> image(imagePtr, AImageDeleter{});

    return image;
}

std::tuple<GLuint, ScopedEGLImage> DecoderData::importAHBtoGLTexture(QRhi* rhi, AHardwareBuffer* ahb)
{
    auto nativeHandles = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles());

    if (!nativeHandles->context)
    {
        NX_WARNING(this, "GL context is invalid");
        return {0, ScopedEGLImage{}};
    }

    EGLClientBuffer clientBuffer = m_eglGetNativeClientBuffer(ahb);
    if (!clientBuffer)
    {
        NX_ERROR(this, "eglGetNativeClientBufferANDROID failed");
        return {0, ScopedEGLImage{}};
    }

    const EGLint attributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};

    ScopedEGLImage image;
    {
        EGLImageKHR imageEgl = m_eglCreateImage(
            m_eglDisplay,
            EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID,
            clientBuffer,
            attributes);

        if (!imageEgl)
        {
            const EGLint error = eglGetError();
            NX_ERROR(this, "eglCreateImage failed %1", eglErrorString(error));
            return {0, ScopedEGLImage{}};
        }

        image = ScopedEGLImage(imageEgl, m_eglDisplay, m_eglDestroyImage);
    }

    NX_ASSERT(QOpenGLContext::currentContext());

    QOpenGLFunctions functions(nativeHandles->context);

    GLuint glTexture = 0;
    functions.glGenTextures(1, &glTexture);

    if (glTexture == 0)
    {
        NX_ERROR(this, "glGenTextures failed: %1", glAllErrors(&functions));
        return {0, ScopedEGLImage{}};
    }

    auto textureCleanup = nx::utils::makeScopeGuard(
        [&]()
        {
            functions.glDeleteTextures(1, &glTexture);
        });

    (void) glAllErrors(&functions); // Clean up error flags from any previous calls.

    functions.glActiveTexture(GL_TEXTURE0);

    auto errStrings = glAllErrors(&functions);
    if (!errStrings.empty())
        NX_WARNING(this, "glActiveTexture failed %1", errStrings);

    functions.glBindTexture(GL_TEXTURE_EXTERNAL_OES, glTexture);
    errStrings = glAllErrors(&functions);
    if (!errStrings.empty())
        NX_WARNING(this, "glBindTexture failed: %1", errStrings);

    m_eglImageTargetTexture2D(GL_TEXTURE_EXTERNAL_OES, image.value());

    errStrings = glAllErrors(&functions);
    if (!errStrings.empty())
        NX_WARNING(this, "glEGLImageTargetTexture2DOES failed: %1", errStrings);

    functions.glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    functions.glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    functions.glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    functions.glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    textureCleanup.disarm();

    return {glTexture, std::move(image)};
}

bool DecoderData::importAHBToVkImage(VkDevice device, AHardwareBuffer* ahb, ImportedImage* out)
{
    QVulkanInstance* inst = QVulkanDefaultInstance::instance();
    if (!NX_ASSERT(inst))
        return false;

    QVulkanDeviceFunctions* f = inst->deviceFunctions(device);

    // Get hardware buffer format.

    VkAndroidHardwareBufferFormatPropertiesANDROID ahbFormatProps{
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID
    };
    VkAndroidHardwareBufferPropertiesANDROID ahbProps{
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
        &ahbFormatProps
    };

    VkResult result = m_vkGetAndroidHardwareBufferProperties(device, ahb, &ahbProps);
    if (result != VK_SUCCESS)
    {
        NX_ERROR(NX_SCOPE_TAG, "vkGetAndroidHardwareBufferPropertiesANDROID failed: %2", result);
        return false;
    }

    // Find the first set bit to use as memoryTypeIndex.
    uint32_t memoryTypeBits = ahbProps.memoryTypeBits;
    int32_t memTypeIndex = -1;
    for (uint32_t i = 0; memoryTypeBits; memoryTypeBits = memoryTypeBits >> 1, ++i)
    {
        if (memoryTypeBits & 0x1)
        {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == -1)
    {
        NX_ERROR(NX_SCOPE_TAG, "No memory type index found");
        return false;
    }

    // Create suitable VkImage.

    VkExternalFormatANDROID externalFormat{VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID};
    if (ahbFormatProps.externalFormat != VK_FORMAT_UNDEFINED)
        externalFormat.externalFormat = ahbFormatProps.externalFormat;

    VkExternalMemoryImageCreateInfo extImg{
        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        &externalFormat
    };
    extImg.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    // Intended usage of the image.
    VkImageUsageFlags usageFlags = 0;

    AHardwareBuffer_Desc desc{};
    AHardwareBuffer_describe(ahb, &desc);

    // Get Vulkan Image usage flag equivalence of AHB usage.
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT)
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (desc.usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT)
        usageFlags |= VK_IMAGE_CREATE_PROTECTED_BIT;

    NX_ASSERT(ahbFormatProps.format == 0);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &extImg};
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {(uint32_t) desc.width, (uint32_t) desc.height, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = ahbFormatProps.format;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usageFlags;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image = VK_NULL_HANDLE;
    result = f->vkCreateImage(device, &imageInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
        NX_ERROR(NX_SCOPE_TAG, "vkCreateImage failed: %2", result);
        return false;
    }

    // Allocate & import memory from AHB
    VkImportAndroidHardwareBufferInfoANDROID ahbInfo{
        VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID
    };
    ahbInfo.buffer = ahb;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        &ahbInfo
    };
    dedicatedAllocInfo.image = image;

    VkMemoryAllocateInfo memAllocInfo{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        &dedicatedAllocInfo
    };
    memAllocInfo.allocationSize = ahbProps.allocationSize;
    memAllocInfo.memoryTypeIndex = memTypeIndex;

    VkDeviceMemory mem = VK_NULL_HANDLE;

    result = f->vkAllocateMemory(device, &memAllocInfo, nullptr, &mem);
    if (result != VK_SUCCESS)
    {
        NX_ERROR(NX_SCOPE_TAG, "vkAllocateMemory failed: %1", result);
        f->vkDestroyImage(device, image, nullptr);
        return false;
    }

    result = f->vkBindImageMemory(device, image, mem, 0);
    if (result != VK_SUCCESS)
    {
        NX_ERROR(NX_SCOPE_TAG, "vkAllocateMemory failed: %1", result);
        f->vkDestroyImage(device, image, nullptr);
        f->vkFreeMemory(device, mem, nullptr);
        return false;
    }

    out->device = device;
    out->ahb = ahb;
    out->image = image;
    out->memory = mem;
    out->width = desc.width;
    out->height = desc.height;
    out->externalFormat = ahbFormatProps.externalFormat;

    return true;
}

class GLVideoFrameTextures: public QVideoFrameTextures
{
public:
    GLVideoFrameTextures()
    {
    }

    virtual ~GLVideoFrameTextures() override
    {
        auto rhi = m_texture ? m_texture->rhi() : nullptr;
        if (!rhi)
            return;

        auto nativeHandles = static_cast<const QRhiGles2NativeHandles*>(rhi->nativeHandles());
        auto glContext = nativeHandles->context;
        if (!glContext)
            return;

        rhi->makeThreadLocalNativeContextCurrent();
        QOpenGLFunctions functions(glContext);
        functions.glDeleteTextures(1, &m_handle);
    }

    virtual QRhiTexture* texture(uint plane) const override
    {
        if (plane > 0)
            return nullptr;

        return m_texture.get();
    }

    ScopedEGLImage m_image;
    GLuint m_handle = 0;
    std::unique_ptr<QRhiTexture> m_texture;
};

class TextureBuffer: public QHwVideoBuffer
{
public:
    TextureBuffer(
            std::shared_ptr<AImage> image,
            std::shared_ptr<DecoderData> decoderData,
            const QVideoFrameFormat& format)
        :
        QHwVideoBuffer(QVideoFrame::RhiTextureHandle),
        m_image(std::move(image)),
        m_decoderData(decoderData),
        m_format(format)
    {
    }

    ~TextureBuffer()
    {
    }

    virtual MapData map(QVideoFrame::MapMode) override
    {
        return {};
    }

    virtual void unmap() override
    {
    }

    virtual std::unique_ptr<QVideoFrameTextures> mapTextures(
        QRhi& rhi, QVideoFrameTexturesUPtr& /*oldTextures*/) override
    {
        AHardwareBuffer* buffer = nullptr;
        media_status_t status = AImage_getHardwareBuffer(m_image.get(), &buffer);

        if (status != AMEDIA_OK)
        {
            NX_ERROR(this, "AImage_getHardwareBuffer failed: %1", amediaErrorString(status));
            return {};
        }

        rhi.makeThreadLocalNativeContextCurrent();

        if (rhi.backend() == QRhi::OpenGLES2)
        {
            auto [glTexture, image] = m_decoderData->importAHBtoGLTexture(&rhi, buffer);
            if (glTexture == 0)
                return {};

            auto textures = std::make_unique<GLVideoFrameTextures>();
            textures->m_image = std::move(image);
            textures->m_handle = glTexture;
            textures->m_texture = createTextureFromHandle(
                format(),
                rhi,
                /*plane*/ 0,
                textures->m_handle);

            return textures;
        }
        else if (rhi.backend() == QRhi::Vulkan)
        {
            auto nativeHandles = static_cast<const QRhiVulkanNativeHandles*>(rhi.nativeHandles());

            auto textures = std::make_unique<VkVideoFrameTextures>();

            if (!m_decoderData->importAHBToVkImage(nativeHandles->dev, buffer, &textures->m_image))
                return {};

            textures->m_texture = createTextureFromHandle(
                format(),
                rhi,
                /*plane*/ 0,
                (quint64) textures->m_image.image,
                /*layout*/ 0,
                /*flags*/ {},
                textures->m_image.externalFormat);

            return textures;
        }
        return {};
    }

    QVideoFrameFormat format() const
    {
        return m_format;
    }

private:
    std::shared_ptr<AImage> m_image;
    std::shared_ptr<DecoderData> m_decoderData;
    QVideoFrameFormat m_format;
};

class AndroidVideoApiEntry: public VideoApiRegistry::Entry
{
public:
    virtual AVHWDeviceType deviceType() const override
    {
        return AV_HWDEVICE_TYPE_MEDIACODEC;
    }

    virtual nx::media::VideoFramePtr makeFrame(
        const AVFrame* frame,
        std::shared_ptr<VideoApiDecoderData> decoderData) const override
    {
        if (!frame)
            return {};

        if (!NX_ASSERT(frame->format == AV_PIX_FMT_MEDIACODEC, "frame format is %1", frame->format))
            return {};

        auto data = std::dynamic_pointer_cast<DecoderData>(decoderData);

        if (!NX_ASSERT(data && data->isInitialized()))
            return {};

        const qint64 startTimeMs = frame->pts == DATETIME_INVALID
            ? DATETIME_INVALID
            : frame->pts / 1000; //< Convert usec to msec.

        auto image = data->imageFromFrame(frame);

        if (!image)
        {
            NX_WARNING(this, "Failed to render frame to AImage");
            return {};
        }

        auto result = std::make_shared<VideoFrame>(
            std::make_unique<TextureBuffer>(
                std::move(image),
                data,
                QVideoFrameFormat(data->frameSize, QVideoFrameFormat::Format_SamplerExternalOES)));

        result->setStartTime(startTimeMs);

        return result;
    }

    virtual std::shared_ptr<VideoApiDecoderData> createDecoderData(
        QRhi* rhi,
        const QSize& frameSize) const override
    {
        return std::make_shared<DecoderData>(rhi, frameSize);
    }

    virtual nx::media::ffmpeg::HwVideoDecoder::InitFunc initFunc(
        QRhi* rhi,
        std::shared_ptr<VideoApiDecoderData> decoderData) const override
    {
        if (!NX_ASSERT(rhi->backend() == QRhi::OpenGLES2 || rhi->backend() == QRhi::Vulkan))
            return {};

        return
            [decoderData=std::dynamic_pointer_cast<DecoderData>(decoderData)](
                AVHWDeviceContext* deviceContext) -> bool
            {
                if (deviceContext->type != AV_HWDEVICE_TYPE_MEDIACODEC)
                {
                    NX_WARNING(NX_SCOPE_TAG,
                        "Unexpected device context type: %1", deviceContext->type);
                    return false;
                }

                auto hwctx = (AVMediaCodecContext*) deviceContext->hwctx;
                if (!hwctx)
                {
                    NX_WARNING(NX_SCOPE_TAG, "MediaCodec context not allocated");
                    return false;
                }

                hwctx->surface = decoderData->getSurface();

                return true;
            };
    }
};

} // namespace

VideoApiRegistry::Entry* getNdkMediaCodecApi()
{
    static AndroidVideoApiEntry apiEntry;
    return &apiEntry;
}

} // namespace nx::media
