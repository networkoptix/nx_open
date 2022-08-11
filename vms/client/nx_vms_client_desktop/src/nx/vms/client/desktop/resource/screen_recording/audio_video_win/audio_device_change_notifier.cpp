// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_device_change_notifier.h"

#include <functional>

#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <atlbase.h>

#include <nx/utils/log/assert.h>

namespace {

QString deviceName(IMMDeviceEnumerator* deviceEnumerator, LPCWSTR deviceId)
{
    if (deviceEnumerator == nullptr || deviceId == nullptr)
    {
        NX_ASSERT(false, "Unexpected null parameter");
        return {};
    }

    CComPtr<IMMDevice> device;
    auto hres = deviceEnumerator->GetDevice(deviceId, &device);
    if (hres != S_OK)
        return {};

    CComPtr<IPropertyStore> deviceProperties;
    hres = device->OpenPropertyStore(STGM_READ, &deviceProperties);
    if (hres != S_OK)
        return {};

    PROPVARIANT nameVariant;
    PropVariantInit(&nameVariant);
    hres = deviceProperties->GetValue(PKEY_Device_FriendlyName, &nameVariant);
    if (hres == S_OK)
    {
        auto result = QString::fromWCharArray(nameVariant.pwszVal);
        PropVariantClear(&nameVariant);
        return result;
    }

    return {};
}

} // namespace

namespace nx::vms::client::desktop {

class AudioDeviceChangeNotifier::DeviceChangeListener: public IMMNotificationClient
{
public:
    using DeviceChangeHandler = std::function<void(LPCWSTR deviceId)>;

    DeviceChangeListener(
        DeviceChangeHandler deviceUnpluggedHandler,
        DeviceChangeHandler deviceNotPresentHandler)
        :
        m_deviceUnpluggedHandler(deviceUnpluggedHandler),
        m_deviceNotPresentHandler(deviceNotPresentHandler)
    {
    }

    // IUnknown interface implementation.

    // DeviceChangeListener instance is exclusively owned by AudioDeviceChangeNotifier instance,
    // thus AddRef() and Release() methods from IUnknown interface are pointless and don't affect
    // reference count which always equals one.

    virtual ULONG AddRef() override
    {
        return 1;
    }

    virtual ULONG Release() override
    {
        return 1;
    }

    virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (riid == IID_IUnknown || riid == __uuidof(IMMNotificationClient))
        {
            *ppvObject = static_cast<IMMNotificationClient*>(this);
            return S_OK;
        }

        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    // IMMNotificationClient interface implementation.

    virtual HRESULT OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override
    {
        return S_OK;
    }

    virtual HRESULT OnDeviceAdded(LPCWSTR) override
    {
        return S_OK;
    }

    virtual HRESULT OnDeviceRemoved(LPCWSTR) override
    {
        return S_OK;
    }

    virtual HRESULT OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override
    {
        if ((dwNewState & DEVICE_STATE_UNPLUGGED) != 0)
        {
            m_deviceUnpluggedHandler(pwstrDeviceId);
            return S_OK;
        }

        if ((dwNewState & DEVICE_STATE_NOTPRESENT) != 0)
        {
            m_deviceNotPresentHandler(pwstrDeviceId);
            return S_OK;
        }

        return S_OK;
    }

    virtual HRESULT OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override
    {
        return S_OK;
    }

private:
    DeviceChangeHandler m_deviceUnpluggedHandler;
    DeviceChangeHandler m_deviceNotPresentHandler;
};

AudioDeviceChangeNotifier::AudioDeviceChangeNotifier():
    base_type()
{
    // Initialize COM library.
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Get audio device enumerator.
    auto hres = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<LPVOID*>(&m_deviceEnumerator));

    if (hres != S_OK)
    {
        CoUninitialize();
        return;
    }

    auto deviceUnpluggedHandler =
        [this](auto deviceId) { emit deviceUnplugged(deviceName(m_deviceEnumerator, deviceId)); };

    auto deviceNotPresentHandler =
        [this](auto deviceId) { emit deviceNotPresent(deviceName(m_deviceEnumerator, deviceId)); };

    m_deviceChangeListener = std::make_unique<DeviceChangeListener>(
        deviceUnpluggedHandler,
        deviceNotPresentHandler);
    hres = m_deviceEnumerator->RegisterEndpointNotificationCallback(m_deviceChangeListener.get());

    if (hres != S_OK)
        m_deviceChangeListener.reset();
}

AudioDeviceChangeNotifier::~AudioDeviceChangeNotifier()
{
    if (m_deviceChangeListener)
        m_deviceEnumerator->UnregisterEndpointNotificationCallback(m_deviceChangeListener.get());

    if (m_deviceEnumerator)
    {
        m_deviceEnumerator->Release();
        CoUninitialize();
    }
}

} // namespace nx::vms::client::desktop
