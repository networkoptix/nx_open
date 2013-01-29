#include "win_audio_helper.h"

#ifdef Q_OS_WIN

#include <InitGuid.h>
#include <Ks.h>
#include <KsMedia.h>
#include <strsafe.h>
#include <devpkey.h>
#include <mmdeviceapi.h>

#include <dsound.h>
#include <MMSystem.h>
#include <mmdeviceapi.h>
#include <FunctionDiscoveryKeys_devpkey.h>
#include <Propvarutil.h>

#include "utils/common/util.h"

#if _MSC_VER < 1600
DEFINE_PROPERTYKEY(PKEY_AudioEndpoint_JackSubType,0x1da5d803,0xd492,0x4edd,0x8c,0x23,0xe0,0xc0,0xff,0xee,0x7f,0x0e,8);
#endif

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

    //PropVariantToGUID(pv, &m_jackSubType);
    const ushort* guidData16 = (const ushort*) pv.pbVal;
    QByteArray arr = QString::fromUtf16(guidData16).toAscii();
    const char* data8 = arr.constData();
    char guidBuffer[24];
    sscanf( data8,
        "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        &guidBuffer[0], &guidBuffer[4], &guidBuffer[6],
        &guidBuffer[8],&guidBuffer[9],&guidBuffer[10],&guidBuffer[11],&guidBuffer[12],&guidBuffer[13],&guidBuffer[14],&guidBuffer[15]);
    memcpy(&m_jackSubType, guidBuffer, 16);

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

    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator, (void**)&m_pMMDeviceEnumerator);
    if (hr != S_OK) return;

    if (deviceName.toLower() == QLatin1String("default"))
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
    if (m_pMMDeviceEnumerator)
        m_pMMDeviceEnumerator->Release();

    CoUninitialize();
}


BOOL CALLBACK enumFunc(HMODULE /*hModule*/, LPCTSTR /*lpszType*/, LPTSTR lpszName,LONG_PTR lParam)
{
    ;
    WinAudioExtendInfo* info = (WinAudioExtendInfo*) lParam;
    if (info->m_iconGroupIndex == 0)
    {
        info->m_iconGroupNum = (short) lpszName;
        return false;
    }
    info->m_iconGroupIndex--;
    return true;
}

QPixmap WinAudioExtendInfo::deviceIcon()
{
    QStringList params = m_iconPath.split(QLatin1Char(','));
    if (params.size() < 2)
        return 0;
    int persent1 = params[0].indexOf(QLatin1Char('%'));
    while (persent1 >= 0)
    {
        int persent2 = params[0].indexOf(QLatin1Char('%'), persent1+1);
        if (persent2 > persent1)
        {
            QString metaVal = params[0].mid(persent1, persent2-persent1+1);
            QString val = QString::fromLocal8Bit(qgetenv(metaVal.mid(1, metaVal.length()-2).toLocal8Bit().constData()));
            params[0].replace(metaVal, val);
        }
        persent1 = params[0].indexOf(QLatin1Char('%'));
    }

    HMODULE library = LoadLibrary((LPCWSTR) params[0].constData());
    if (!library)
        return false;
    int resNumber = params[1].toInt();
    if (resNumber < 0) {
        resNumber = -resNumber;
    }
    else
    {
        m_iconGroupIndex = resNumber;
        m_iconGroupNum = 0;
        EnumResourceNames(library, RT_GROUP_ICON, enumFunc, (LONG_PTR) this);
        resNumber = m_iconGroupNum;
    }
    HICON hIcon = LoadIcon(library, MAKEINTRESOURCE(resNumber));
    QPixmap rez;
    if (hIcon)
        rez = QPixmap::fromWinHICON( hIcon);
    DestroyIcon(hIcon);
    return rez;
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
    return m_fullName.toLower().contains(QLatin1String("microphone"));
}

#endif // Q_OS_WIN
