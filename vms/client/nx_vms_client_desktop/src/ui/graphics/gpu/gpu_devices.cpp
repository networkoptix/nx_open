// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gpu_devices.h"

#include <algorithm>
#include <iterator>

#if defined(Q_OS_WIN)
#include <dxgi1_3.h>
#include <wrl/client.h>
#endif

#include <QtCore/QStringList>
#include <QtGui/rhi/qrhi.h>

#if QT_CONFIG(vulkan)
#include <QtGui/QVulkanFunctions>
#include <QtGui/QVulkanInstance>
#include <QtGui/private/qvulkandefaultinstance_p.h>
#endif

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop::gpu {

#if defined(Q_OS_WIN)

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

        result.append({
            .name = QString::fromWCharArray(desc.Description),
            .supportsVideoDecode = !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
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
    uint32_t devicesCount = 0;
    f->vkEnumeratePhysicalDevices(inst->vkInstance(), &devicesCount, nullptr);
    QList<VkPhysicalDevice> devices(devicesCount, {});
    f->vkEnumeratePhysicalDevices(inst->vkInstance(), &devicesCount, devices.data());

    for (int i = 0; i < devices.size(); ++i)
    {
        VkPhysicalDeviceProperties properties;
        f->vkGetPhysicalDeviceProperties(devices[i], &properties);

        uint32_t extCount = 0;
        f->vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extCount, nullptr);
        QList<VkExtensionProperties> extensions(extCount, {});
        f->vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extCount, extensions.data());

        bool supportsVideoDecode = false;
        for (const VkExtensionProperties& extension: extensions)
        {
            // TODO: Gather supported codecs.
            if (QLatin1String(extension.extensionName) == "VK_KHR_video_decode_queue")
            {
                supportsVideoDecode = true;
                break;
            }
        }

        result.append({
            .name = properties.deviceName,
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

void selectDevice(QSGRendererInterface::GraphicsApi api, const QString& name)
{
    QList<DeviceInfo> devices;
    const char* envName = nullptr;
    const char* logName = "";

    switch (api)
    {
        case QSGRendererInterface::Direct3D11:
        case QSGRendererInterface::Direct3D12:
            #if defined(Q_OS_WIN)
                logName = "D3D";
                devices = listDevicesD3D();
                envName = "QT_D3D_ADAPTER_INDEX";
            #endif
            break;
        case QSGRendererInterface::Vulkan:
            #if QT_CONFIG(vulkan)
                logName = "VK";
                devices = listDevicesVulkan();
                envName = "QT_VK_PHYSICAL_DEVICE_INDEX";
            #endif
            break;
        default:
            return;
    }

    for (int i = 0; i < devices.size(); ++i)
    {
        const DeviceInfo& device = devices[i];
        NX_DEBUG(
            NX_SCOPE_TAG,
            "%1 %2: %3 Supports HW video decode: %4",
            logName, i, device.name, device.supportsVideoDecode);
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

    if (foundIt == devices.cend())
        return;

    const int adapterIndex = std::distance(devices.cbegin(), foundIt);

    NX_DEBUG(NX_SCOPE_TAG, "Selected GPU #%1", adapterIndex);

    qputenv(envName, QByteArray::number(adapterIndex));
}

} // nx::vms::client::desktop::gpu
