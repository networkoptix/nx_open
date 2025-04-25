// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gpu_devices.h"

#include <algorithm>
#include <iterator>

#if defined(Q_OS_WIN)
#include <dxgi1_3.h>
#include <d3d11_4.h>
#include <wrl/client.h>
#endif

#include <QtCore/QStringList>
#include <QtGui/rhi/qrhi.h>

#if QT_CONFIG(vulkan)
#include <QtGui/QVulkanFunctions>
#include <QtGui/QVulkanInstance>
#include <QtGui/private/qvulkandefaultinstance_p.h>
#endif

#if defined(Q_OS_LINUX)
#include <QtGui/QOpenGLFunctions>
#include <EGL/egl.h>
#include <EGL/eglext.h>
// Avoid name conflict with template arguments in the headers below.
#if defined(None)
    #undef None
#endif
#endif

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop::gpu {

#if defined (Q_OS_LINUX)
QList<DeviceInfo> listDevicesEGL()
{
    const char* extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!extensions || !strstr(extensions, "EGL_EXT_explicit_device"))
    {
        NX_DEBUG(NX_SCOPE_TAG, "EGL_EXT_explicit_device is not supported");
        return {};
    }

    auto queryDevices = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(
        eglGetProcAddress("eglQueryDevicesEXT"));

    auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYPROC>(
        eglGetProcAddress("eglGetPlatformDisplay"));

    auto queryDeviceString = reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(
        eglGetProcAddress("eglQueryDeviceStringEXT"));

    if (!NX_ASSERT(queryDevices) || !NX_ASSERT(getPlatformDisplay))
        return {};

    std::vector<EGLDeviceEXT> devices;
    EGLint deviceCount = 0;

    // Get the number of supported devices in the system.
    if (!queryDevices(0, nullptr, &deviceCount) || deviceCount < 1)
    {
        NX_ERROR(NX_SCOPE_TAG, "Getting the number of devices with eglQueryDevicesEXT failed");
        return {};
    }
    devices.resize(deviceCount);

    // Get the list of supported devices.
    if (!queryDevices(devices.size(), devices.data(), &deviceCount) || deviceCount < 1)
    {
        NX_ERROR(NX_SCOPE_TAG, "Getting the devices with eglQueryDevicesEXT failed");
        return {};
    }
    devices.resize(deviceCount);

    QList<DeviceInfo> result;
    for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex)
    {
        const EGLDeviceEXT dev = devices.at(deviceIndex);

        auto deviceExtensions = queryDeviceString(dev, EGL_EXTENSIONS);

        const bool isSoftware = deviceExtensions
            && strstr(deviceExtensions, "EGL_MESA_device_software");

        if (deviceExtensions && strstr(deviceExtensions, "EGL_EXT_device_query_name"))
        {
            const char* eglVendor = queryDeviceString(dev, EGL_VENDOR);
            const EGLint kEGL_RENDERER_EXT = 0x335F;
            const char* eglRenderer = queryDeviceString(dev, kEGL_RENDERER_EXT);

            result.append({
                .index = deviceIndex,
                .name = QString("%1 %2").arg(eglVendor).arg(eglRenderer),
                .additionalInfo = deviceExtensions,
                .supportsVideoDecode = !isSoftware
            });

            continue;
        }

        EGLAttrib attribList[] = {
            EGL_DEVICE_EXT, (EGLAttrib) dev,
            EGL_NONE
        };

        auto eglDisplay = getPlatformDisplay(
            EGL_PLATFORM_X11_EXT,
            EGL_DEFAULT_DISPLAY,
            attribList);

        if (eglDisplay == EGL_NO_DISPLAY)
            continue;

        const bool alreadyInitialized = !!eglQueryString(eglDisplay, EGL_VENDOR);

        EGLint major = 0;
        EGLint minor = 0;
        if (alreadyInitialized || eglInitialize(eglDisplay, &major, &minor))
        {
            auto terminateDisplay = nx::utils::makeScopeGuard(
                [alreadyInitialized, eglDisplay]()
                {
                    if (!alreadyInitialized)
                        eglTerminate(eglDisplay);
                });

            if (!eglBindAPI(EGL_OPENGL_API))
                continue;

            const EGLint attribList[] = {
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_NONE
            };
            EGLConfig config;
            EGLint numConfig = 0;
            if (!eglChooseConfig(eglDisplay, attribList, &config, 1, &numConfig))
                continue;

            auto eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, nullptr);

            if (eglContext == EGL_NO_CONTEXT)
                continue;

            auto destroyContext = nx::utils::makeScopeGuard(
                [eglDisplay, eglContext]()
                {
                    eglDestroyContext(eglDisplay, eglContext);
                });

            if (!eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext))
                continue;

            auto releaseContext = nx::utils::makeScopeGuard(
                [eglDisplay]()
                {
                    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                });

                result.append({
                    .index = deviceIndex,
                    .name = QString("%1 %2")
                        .arg((const char*)glGetString(GL_VENDOR))
                        .arg((const char*)glGetString(GL_RENDERER)),
                    .additionalInfo = QString("%1 %2")
                        .arg(deviceExtensions)
                        .arg(glGetString(GL_EXTENSIONS)),
                    .supportsVideoDecode = !isSoftware
                });
        }
    }

    return result;
}
#endif

#if defined(Q_OS_WIN)
// Use the test shader from Qt.
static constexpr auto g_testVertexShader = std::to_array<BYTE>({
     68,  88,  66,  67,  75, 198,
     18, 149, 172, 244, 247, 123,
     98,  31, 128, 185,  22, 199,
    182, 233,   1,   0,   0,   0,
    140,   3,   0,   0,   5,   0,
      0,   0,  52,   0,   0,   0,
     60,   1,   0,   0, 136,   1,
      0,   0, 224,   1,   0,   0,
    240,   2,   0,   0,  82,  68,
     69,  70,   0,   1,   0,   0,
      1,   0,   0,   0,  96,   0,
      0,   0,   1,   0,   0,   0,
     60,   0,   0,   0,   0,   5,
    254, 255,   0,   1,   0,   0,
    216,   0,   0,   0,  82,  68,
     49,  49,  60,   0,   0,   0,
     24,   0,   0,   0,  32,   0,
      0,   0,  40,   0,   0,   0,
     36,   0,   0,   0,  12,   0,
      0,   0,   0,   0,   0,   0,
     92,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      1,   0,   0,   0,   1,   0,
      0,   0,  98, 117, 102,   0,
     92,   0,   0,   0,   1,   0,
      0,   0, 120,   0,   0,   0,
     64,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
    160,   0,   0,   0,   0,   0,
      0,   0,  64,   0,   0,   0,
      2,   0,   0,   0, 180,   0,
      0,   0,   0,   0,   0,   0,
    255, 255, 255, 255,   0,   0,
      0,   0, 255, 255, 255, 255,
      0,   0,   0,   0, 117,  98,
    117, 102,  95, 109, 118, 112,
      0, 102, 108, 111,  97, 116,
     52, 120,  52,   0, 171, 171,
      2,   0,   3,   0,   4,   0,
      4,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0, 169,   0,   0,   0,
     77, 105,  99, 114, 111, 115,
    111, 102, 116,  32,  40,  82,
     41,  32,  72,  76,  83,  76,
     32,  83, 104,  97, 100, 101,
    114,  32,  67, 111, 109, 112,
    105, 108, 101, 114,  32,  49,
     48,  46,  49,   0,  73,  83,
     71,  78,  68,   0,   0,   0,
      2,   0,   0,   0,   8,   0,
      0,   0,  56,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   3,   0,   0,   0,
      0,   0,   0,   0,  15,  15,
      0,   0,  56,   0,   0,   0,
      1,   0,   0,   0,   0,   0,
      0,   0,   3,   0,   0,   0,
      1,   0,   0,   0,   7,   7,
      0,   0,  84,  69,  88,  67,
     79,  79,  82,  68,   0, 171,
    171, 171,  79,  83,  71,  78,
     80,   0,   0,   0,   2,   0,
      0,   0,   8,   0,   0,   0,
     56,   0,   0,   0,   0,   0,
      0,   0,   1,   0,   0,   0,
      3,   0,   0,   0,   0,   0,
      0,   0,  15,   0,   0,   0,
     68,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      3,   0,   0,   0,   1,   0,
      0,   0,   7,   8,   0,   0,
     83,  86,  95,  80, 111, 115,
    105, 116, 105, 111, 110,   0,
     84,  69,  88,  67,  79,  79,
     82,  68,   0, 171, 171, 171,
     83,  72,  69,  88,   8,   1,
      0,   0,  80,   0,   1,   0,
     66,   0,   0,   0, 106,   8,
      0,   1,  89,   0,   0,   4,
     70, 142,  32,   0,   0,   0,
      0,   0,   4,   0,   0,   0,
     95,   0,   0,   3, 242,  16,
     16,   0,   0,   0,   0,   0,
     95,   0,   0,   3, 114,  16,
     16,   0,   1,   0,   0,   0,
    103,   0,   0,   4, 242,  32,
     16,   0,   0,   0,   0,   0,
      1,   0,   0,   0, 101,   0,
      0,   3, 114,  32,  16,   0,
      1,   0,   0,   0, 104,   0,
      0,   2,   1,   0,   0,   0,
     56,   0,   0,   8, 242,   0,
     16,   0,   0,   0,   0,   0,
     86,  21,  16,   0,   0,   0,
      0,   0,  70, 142,  32,   0,
      0,   0,   0,   0,   1,   0,
      0,   0,  50,   0,   0,  10,
    242,   0,  16,   0,   0,   0,
      0,   0,   6,  16,  16,   0,
      0,   0,   0,   0,  70, 142,
     32,   0,   0,   0,   0,   0,
      0,   0,   0,   0,  70,  14,
     16,   0,   0,   0,   0,   0,
     50,   0,   0,  10, 242,   0,
     16,   0,   0,   0,   0,   0,
    166,  26,  16,   0,   0,   0,
      0,   0,  70, 142,  32,   0,
      0,   0,   0,   0,   2,   0,
      0,   0,  70,  14,  16,   0,
      0,   0,   0,   0,  50,   0,
      0,  10, 242,  32,  16,   0,
      0,   0,   0,   0, 246,  31,
     16,   0,   0,   0,   0,   0,
     70, 142,  32,   0,   0,   0,
      0,   0,   3,   0,   0,   0,
     70,  14,  16,   0,   0,   0,
      0,   0,  54,   0,   0,   5,
    114,  32,  16,   0,   1,   0,
      0,   0,  70,  18,  16,   0,
      1,   0,   0,   0,  62,   0,
      0,   1,  83,  84,  65,  84,
    148,   0,   0,   0,   6,   0,
      0,   0,   1,   0,   0,   0,
      0,   0,   0,   0,   4,   0,
      0,   0,   4,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   1,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   1,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,
      0,   0
});

QList<DeviceInfo> listDevicesD3D()
{
    using namespace Microsoft::WRL;

    QList<DeviceInfo> result;

    ComPtr<IDXGIFactory2> dxgiFactory;
    const HRESULT hr = CreateDXGIFactory2(
        0, __uuidof(IDXGIFactory2), reinterpret_cast<void **>(dxgiFactory.GetAddressOf()));
    if (FAILED(hr))
    {
        NX_WARNING(NX_SCOPE_TAG,
            "CreateDXGIFactory2() failed to create DXGI factory: 0x%1", QString::number(hr, 16));
        return {};
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (int adapterIndex = 0;
        dxgiFactory->EnumAdapters1(UINT(adapterIndex), adapter.ReleaseAndGetAddressOf())
            != DXGI_ERROR_NOT_FOUND;
        ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        const auto description = QString::fromWCharArray(desc.Description);

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_1_0_CORE;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        auto hr = D3D11CreateDevice(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            /*Software*/ nullptr,
            /*Flags*/ 0,
            /*pFeatureLevels*/ nullptr,
            /*featureLevels*/ 0,
            D3D11_SDK_VERSION,
            device.GetAddressOf(),
            &featureLevel,
            context.GetAddressOf());

        if (FAILED(hr))
        {
            NX_DEBUG(NX_SCOPE_TAG,
                "D3D11CreateDevice() failed to create %1 device: 0x%2",
                description,
                QString::number(hr, 16));
            continue;
        }

        // Do the same checks as Qt to ensure we can use the device.

        // Check Direct3D 11.1 support.
        ComPtr<ID3D11DeviceContext1> context1;
        if (FAILED(context.As(&context1)))
        {
            NX_DEBUG(NX_SCOPE_TAG, "ID3D11DeviceContext1 is not supported by %1", description);
            continue;
        }

        // Check Shader Model 5.0 support.
        ComPtr<ID3D11VertexShader> testShader;
        if (FAILED(device->CreateVertexShader(
            &g_testVertexShader,
            g_testVertexShader.size(),
            nullptr,
            testShader.GetAddressOf())))
        {
            NX_DEBUG(NX_SCOPE_TAG, "Shader Model 5.0 is not supported by %1", description);
            continue;
        }

        // Check if the device supports constant buffer offsetting.
        D3D11_FEATURE_DATA_D3D11_OPTIONS features = {};
        if (FAILED(device->CheckFeatureSupport(
                D3D11_FEATURE_D3D11_OPTIONS,
                &features,
                sizeof(features)))
            || !features.ConstantBufferOffsetting)
        {
            NX_DEBUG(NX_SCOPE_TAG, "ConstantBufferOffsetting is not supported by %1", description);
            continue;
        }

        ComPtr<ID3D11VideoDevice> videoDevice;
        const bool supportsVideoDevice = SUCCEEDED(device.As(&videoDevice));

        result.append({
            .index = adapterIndex,
            .name = description,
            .additionalInfo = QString("D3D_FEATURE_LEVEL=0x%1").arg(featureLevel, 0, 16),
            .supportsVideoDecode = supportsVideoDevice
        });
    }

    return result;
}
#endif

#if QT_CONFIG(vulkan)
QList<DeviceInfo> listDevicesVulkan()
{
    QList<DeviceInfo> result;

    QVulkanInstance* inst = QVulkanDefaultInstance::instance();
    if (!inst)
        return {};

    QVulkanFunctions *f = inst->functions();
    uint32_t deviceCount = 0;
    f->vkEnumeratePhysicalDevices(inst->vkInstance(), &deviceCount, nullptr);
    QList<VkPhysicalDevice> devices(deviceCount, VkPhysicalDevice{});
    f->vkEnumeratePhysicalDevices(inst->vkInstance(), &deviceCount, devices.data());

    for (int i = 0; i < devices.size(); ++i)
    {
        VkPhysicalDeviceProperties properties;
        f->vkGetPhysicalDeviceProperties(devices[i], &properties);

        uint32_t extCount = 0;
        f->vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extCount, nullptr);
        QList<VkExtensionProperties> extensions(extCount, {});
        f->vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extCount, extensions.data());

        QStringList extensionsList;

        bool supportsVideoDecode = false;
        for (const VkExtensionProperties& extension: extensions)
        {
            // TODO: Gather supported codecs.
            if (QLatin1String(extension.extensionName) == "VK_KHR_video_decode_queue")
                supportsVideoDecode = true;

            extensionsList.append(
                QString("%1=%2").arg(extension.extensionName).arg(extension.specVersion));
        }

        result.append({
            .index = i,
            .name = properties.deviceName,
            .additionalInfo = extensionsList.join(' '),
            .supportsVideoDecode = supportsVideoDecode
        });
    }

    return result;
}
#endif

bool isVulkanVideoSupported()
{
    #if QT_CONFIG(vulkan)
        auto devices = listDevicesVulkan();
        const auto deviceIt = std::find_if(
            devices.cbegin(),
            devices.cend(),
            [](const auto& dev) { return dev.supportsVideoDecode; });
        return deviceIt != devices.cend();
    #else
        return false;
    #endif
}

DeviceInfo selectDevice(GraphicsApi api, const QString& name)
{
    QList<DeviceInfo> devices;
    const char* envName = nullptr;
    const char* logName = "";

    switch (api)
    {
        case GraphicsApi::direct3d11:
        case GraphicsApi::direct3d12:
            #if defined(Q_OS_WIN)
                logName = "D3D";
                devices = listDevicesD3D();
                envName = "QT_D3D_ADAPTER_INDEX";
            #endif
            break;
        case GraphicsApi::vulkan:
            #if QT_CONFIG(vulkan)
                logName = "VK";
                devices = listDevicesVulkan();
                envName = "QT_VK_PHYSICAL_DEVICE_INDEX";
            #endif
            break;
        case GraphicsApi::opengl:
            #if defined (Q_OS_LINUX)
                logName = "EGL";
                devices = listDevicesEGL();
                envName = "QT_EGL_DEVICE_INDEX";
            #endif
            break;
        default:
            return {};
    }

    if (!envName)
        return {};

    for (int i = 0; i < devices.size(); ++i)
    {
        const DeviceInfo& device = devices[i];
        NX_DEBUG(NX_SCOPE_TAG, "%1 %2: %3", logName, device.index, device.name);
        NX_DEBUG(NX_SCOPE_TAG, "  HW video decode: %1", device.supportsVideoDecode);
        NX_VERBOSE(NX_SCOPE_TAG, "  %1", device.additionalInfo);
    }

    QList<DeviceInfo>::const_iterator foundIt;

    if (name.isEmpty())
    {
        foundIt = std::find_if(
            devices.cbegin(), devices.cend(), [](auto dev) { return dev.supportsVideoDecode; });
    }
    else
    {
        const auto lowerName = name.toLower();
        foundIt = std::find_if(
            devices.cbegin(),
            devices.cend(),
            [lowerName](auto dev) { return dev.name.toLower().contains(lowerName); });
    }

    int adapterIndex = foundIt == devices.cend()
        ? (devices.empty() ? 0 : devices.front().index)
        : foundIt->index;

    // Allow to override selected index with Qt specific environment variables.
    bool ok = false;
    int envRequestedIndex = qgetenv(envName).toInt(&ok);
    if (!ok)
        envRequestedIndex = -1;

    if (envRequestedIndex >= 0 && envRequestedIndex != adapterIndex)
    {
        adapterIndex = envRequestedIndex;
        NX_DEBUG(NX_SCOPE_TAG, "Selected GPU #%1 via env %2", adapterIndex, envName);
    }
    else
    {
        NX_DEBUG(NX_SCOPE_TAG, "Selected GPU #%1", adapterIndex);
    }

    qputenv(envName, QByteArray::number(adapterIndex));

    // Return the selected adapter info.

    foundIt = std::find_if(
        devices.cbegin(),
        devices.cend(),
        [adapterIndex](auto dev) { return dev.index == adapterIndex; });

    if (foundIt != devices.cend())
        return *foundIt;

    return {};
}

} // nx::vms::client::desktop::gpu
