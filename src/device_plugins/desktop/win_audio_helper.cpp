#include <INITGUID.H>
#include <ks.h>
#include <ksmedia.h>
#include <Strsafe.h>
#include <devpkey.h>
#include <mmdeviceapi.h>
#include "win_audio_helper.h"

#include <dsound.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include "util.h"
#include "Propvarutil.h"

DEFINE_PROPERTYKEY(PKEY_AudioEndpoint_JackSubType,0x1da5d803,0xd492,0x4edd,0x8c,0x23,0xe0,0xc0,0xff,0xee,0x7f,0x0e,8);

bool WinAudioExtendInfo::getDeviceInfo(IMMDevice *pMMDevice, bool isDefault)
{
    IPropertyStore *pPropertyStore;
    HRESULT hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
    if (hr != S_OK)
        return false;

    PROPVARIANT pv; PropVariantInit(&pv);
    hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &pv);
    if (hr != S_OK) 
        return false;

    QString name = QString::fromUtf16(reinterpret_cast<const ushort *>(pv.piVal));
    if (!isDefault && !name.startsWith(m_deviceName))
        return false;
    
    if (!isDefault)
        m_fullName = !name.isEmpty() ? name : m_deviceName;
    else
        m_fullName = m_deviceName;

    hr = pPropertyStore->GetValue(PKEY_AudioEndpoint_JackSubType, &pv);
    if (hr != S_OK)
        return false;

    PropVariantToGUID(pv, &m_jackSubType);

    hr = pPropertyStore->GetValue(PKEY_DeviceClass_IconPath, &pv);
    if (hr != S_OK) 
        return false;
    m_iconPath = QString::fromUtf16(reinterpret_cast<const ushort *>(pv.piVal));
    
    return true;
}

WinAudioExtendInfo::WinAudioExtendInfo(const QString& deviceName)
{
    m_deviceName = deviceName;
    IMMDeviceCollection *ppDevices;
    HRESULT hr;
    IMMDevice *pMMDevice;

    CoInitialize(0);

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, 
        __uuidof(IMMDeviceEnumerator),
        (void**)&m_pMMDeviceEnumerator
        );
    if (hr != S_OK) return;

    if (deviceName.toLower() == "default")
    {
        m_pMMDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pMMDevice);
        getDeviceInfo(pMMDevice, true);
    }


    hr = m_pMMDeviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &ppDevices);
    if (hr != S_OK) return;

    UINT count;
    hr = ppDevices->GetCount(&count);
    if (hr != S_OK) return;
    for (UINT i = 0; i < count; i++) 
    {
        hr = ppDevices->Item(i, &pMMDevice);
        if (hr != S_OK) return;
        if (getDeviceInfo(pMMDevice, false))
            break;
    }
}

WinAudioExtendInfo::~WinAudioExtendInfo()
{
    m_pMMDeviceEnumerator->Release();
}

QPixmap WinAudioExtendInfo::deviceIcon() const
{
    QStringList params = m_iconPath.split(',');
    if (params.size() < 2)
        return 0;
    int persent1 = params[0].indexOf('%');
    while (persent1 >= 0)
    {
        int persent2 = params[0].indexOf('%', persent1+1);
        if (persent2 > persent1)
        {
            QString metaVal = params[0].mid(persent1, persent2-persent1+1);
            QString val = getenv(metaVal.mid(1, metaVal.length()-2).toLocal8Bit().constData());
            params[0].replace(metaVal, val);
        }
        persent1 = params[0].indexOf('%');
    }

    HMODULE library = LoadLibrary((LPCWSTR) params[0].constData());
    if (library < 0)
        return false;
    int resNumber = qAbs(params[1].toInt());

    HICON hIcon = LoadIcon(library, MAKEINTRESOURCE(resNumber));
    if (hIcon)
        return QPixmap::fromWinHICON( hIcon);
    else
        return QPixmap();
}


bool WinAudioExtendInfo::isMicrophone() const
{
    const GUID MicrophoneGUIDS[] = 
    {
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

    for (int i = 0; i < arraysize(MicrophoneGUIDS); ++i)
    {
        if (MicrophoneGUIDS[i] == m_jackSubType)
            return true;
    }

    // device type unknown. try to check device name
    return m_fullName.toLower().contains("microphone");
}
