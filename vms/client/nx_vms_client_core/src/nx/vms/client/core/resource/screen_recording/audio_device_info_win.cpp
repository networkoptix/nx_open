// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_device_info_win.h"

#include <vector>

#include <Windows.h>
#include <InitGuid.h>
#include <Ks.h>
#include <KsMedia.h>
#include <strsafe.h>
#include <devpkey.h>
#include <mmeapi.h>
#include <mmdeviceapi.h>
#include <dsound.h>
#include <MMSystem.h>
#include <FunctionDiscoveryKeys_devpkey.h>

#include <QtCore/QUuid>

#include <nx/utils/std/algorithm.h>

#pragma comment(lib, "ole32.lib")

namespace nx::vms::client::core {

struct WinAudioDeviceInfo::Private
{
    void init(const QString& deviceName);
    void deinit();

    bool getDeviceInfo(IMMDevice* pMMDevice, bool isDefault);
    QPixmap getDeviceIcon() const;

    bool isMicrophone() const;

    mutable int iconGroupIndex = 0;
    mutable int iconGroupNum = 0;
    QString deviceName;
    QString fullName;
    GUID jackSubType = {};
    QString iconPath;
    IMMDeviceEnumerator* deviceEnumerator = nullptr;

    friend BOOL CALLBACK enumFunc(
        HMODULE hModule,
        LPCTSTR lpszType,
        LPTSTR lpszName,
        LONG_PTR lParam);
};

void WinAudioDeviceInfo::Private::init(const QString& deviceName)
{
    this->deviceName = deviceName;

    IMMDeviceCollection* ppDevices;
    HRESULT hr;
    IMMDevice* pMMDevice;

    CoInitialize(0);

    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        nullptr,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**)&deviceEnumerator);
    if (hr != S_OK)
        return;

    if (deviceName.toLower() == "default")
    {
        deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pMMDevice);
        getDeviceInfo(pMMDevice, true);
    }

    hr = deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &ppDevices);
    if (hr != S_OK)
        return;

    UINT count;
    hr = ppDevices->GetCount(&count);
    if (hr != S_OK)
        return;
    for (UINT i = 0; i < count; i++)
    {
        hr = ppDevices->Item(i, &pMMDevice);
        if (hr != S_OK)
            return;
        if (getDeviceInfo(pMMDevice, false))
            break;
    }
}

void WinAudioDeviceInfo::Private::deinit()
{
    if (deviceEnumerator)
        deviceEnumerator->Release();

    CoUninitialize();
}

bool WinAudioDeviceInfo::Private::getDeviceInfo(IMMDevice* pMMDevice, bool isDefault)
{
    IPropertyStore* pPropertyStore;
    HRESULT hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
    if (hr != S_OK)
        return false;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &pv);
    if (hr != S_OK)
        return false;

    QString name = QString::fromUtf16(reinterpret_cast<const ushort *>(pv.piVal));
    if (!isDefault && !name.startsWith(deviceName))
        return false;

    if (!isDefault)
        fullName = !name.isEmpty() ? name : deviceName;
    else
        fullName = deviceName;

    hr = pPropertyStore->GetValue(PKEY_AudioEndpoint_JackSubType, &pv);
    if (hr != S_OK)
        return false;

    const ushort* guidData16 = (const ushort*)pv.pbVal;
    jackSubType = QUuid(QString::fromUtf16(guidData16));

    hr = pPropertyStore->GetValue(PKEY_DeviceClass_IconPath, &pv);
    if (hr != S_OK)
        return false;
    iconPath = QString::fromUtf16(reinterpret_cast<const ushort *>(pv.piVal));

    return true;
}

BOOL CALLBACK enumFunc(HMODULE /*hModule*/, LPCTSTR /*lpszType*/, LPTSTR lpszName, LONG_PTR lParam)
{
    const auto info = (WinAudioDeviceInfo::Private*)lParam;
    if (info->iconGroupIndex == 0)
    {
        info->iconGroupNum = (short)lpszName;
        return false;
    }
    info->iconGroupIndex--;
    return true;
}

QPixmap WinAudioDeviceInfo::Private::getDeviceIcon() const
{
    static const QChar kPercent('%');
    QStringList params = iconPath.split(',');
    if (params.size() < 2)
        return QPixmap();

    int percent1 = params[0].indexOf(kPercent);
    while (percent1 >= 0)
    {
        const int percent2 = params[0].indexOf(kPercent, percent1 + 1);
        if (percent2 > percent1)
        {
            QString metaVal = params[0].mid(percent1, percent2 - percent1 + 1);
            QString val = QString::fromLocal8Bit(
                qgetenv(metaVal.mid(1, metaVal.length() - 2).toLatin1().constData()));
            params[0].replace(metaVal, val);
        }
        percent1 = params[0].indexOf(kPercent);
    }

    const HMODULE library = LoadLibrary((LPCWSTR)params[0].constData());
    if (!library)
        return QPixmap();

    int resNumber = params[1].toInt();
    if (resNumber < 0)
    {
        resNumber = -resNumber;
    }
    else
    {
        iconGroupIndex = resNumber;
        iconGroupNum = 0;
        EnumResourceNames(library, RT_GROUP_ICON, enumFunc, (LONG_PTR)this);
        resNumber = iconGroupNum;
    }

    HICON hIcon = LoadIcon(library, MAKEINTRESOURCE(resNumber));
    QPixmap result;
    if (hIcon)
        result = QPixmap::fromImage(QImage::fromHICON(hIcon));
    DestroyIcon(hIcon);
    return result;
}

bool WinAudioDeviceInfo::Private::isMicrophone() const
{
    static const std::vector<GUID> kMicrophoneGuids{
        KSNODETYPE_MICROPHONE,
        KSNODETYPE_DESKTOP_MICROPHONE,
        KSNODETYPE_PERSONAL_MICROPHONE,
        KSNODETYPE_OMNI_DIRECTIONAL_MICROPHONE,
        KSNODETYPE_MICROPHONE_ARRAY,
        KSNODETYPE_PROCESSING_MICROPHONE_ARRAY,
        KSNODETYPE_HEADSET_MICROPHONE,
        KSAUDFNAME_ALTERNATE_MICROPHONE,
        KSNODETYPE_PROCESSING_MICROPHONE_ARRAY,
        KSAUDFNAME_LINE_IN_VOLUME,
        KSAUDFNAME_LINE_IN
    };

    if (nx::utils::contains(kMicrophoneGuids, jackSubType))
        return true;

    // Device type unknown. try to check device name.
    return fullName.toLower().contains("microphone");
}

// -------------------------------------------------------------------------- //
// WinAudioDeviceInfo
// -------------------------------------------------------------------------- //
WinAudioDeviceInfo::WinAudioDeviceInfo(const QString& deviceName):
    d(new Private())
{
    d->init(deviceName);
}

WinAudioDeviceInfo::~WinAudioDeviceInfo()
{
    d->deinit();
}

QString WinAudioDeviceInfo::fullName() const
{
    return d->fullName;
}

QPixmap WinAudioDeviceInfo::deviceIcon() const
{
    return d->getDeviceIcon();
}

bool WinAudioDeviceInfo::isMicrophone() const
{
    return d->isMicrophone();
}

} // namespace nx::vms::client::core
