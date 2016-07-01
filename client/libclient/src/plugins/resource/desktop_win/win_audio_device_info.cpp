#include "win_audio_device_info.h"

#ifdef Q_OS_WIN

#include <Windows.h>
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

#pragma comment(lib, "ole32.lib")

/* This one is private API, but it is exported from QtGui. */
extern QPixmap qt_pixmapFromWinHICON(HICON icon); // TODO: #Elric use QtWin::fromHICON

// -------------------------------------------------------------------------- //
// QnWinAudioDeviceInfoPrivate
// -------------------------------------------------------------------------- //
class QnWinAudioDeviceInfoPrivate {
public:
    QnWinAudioDeviceInfoPrivate(): iconGroupIndex(0), iconGroupNum(0), deviceEnumerator(NULL) {}
    virtual ~QnWinAudioDeviceInfoPrivate() {}

    void init(const QString &deviceName);
    void deinit();

    bool getDeviceInfo(IMMDevice *pMMDevice, bool isDefault);
    QPixmap getDeviceIcon();

    bool getIsMicrophone() const;

public:
    int iconGroupIndex;
    int iconGroupNum;
    QString deviceName;
    QString fullName;
    GUID jackSubType;
    QString iconPath;
    IMMDeviceEnumerator *deviceEnumerator;

    friend BOOL CALLBACK enumFunc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam);
};

void QnWinAudioDeviceInfoPrivate::init(const QString &deviceName) {
    this->deviceName = deviceName;
    
    IMMDeviceCollection *ppDevices;
    HRESULT hr;
    IMMDevice *pMMDevice;

    CoInitialize(0);

    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
        IID_IMMDeviceEnumerator, (void**)&deviceEnumerator);
    if (hr != S_OK) return;

    if (deviceName.toLower() == QLatin1String("default"))
    {
        deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pMMDevice);
        getDeviceInfo(pMMDevice, true);
    }


    hr = deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &ppDevices);
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

void QnWinAudioDeviceInfoPrivate::deinit() {
    if (deviceEnumerator)
        deviceEnumerator->Release();

    CoUninitialize();
}

bool QnWinAudioDeviceInfoPrivate::getDeviceInfo(IMMDevice *pMMDevice, bool isDefault) {
    IPropertyStore *pPropertyStore;
    HRESULT hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
    if (hr != S_OK)
        return false;

    PROPVARIANT pv; PropVariantInit(&pv);
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

    //PropVariantToGUID(pv, &m_jackSubType);
    const ushort* guidData16 = (const ushort*) pv.pbVal;
    QByteArray arr = QString::fromUtf16(guidData16).toLatin1();
    const char* data8 = arr.constData();
    char guidBuffer[24];
    sscanf( data8,
        "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        &guidBuffer[0], &guidBuffer[4], &guidBuffer[6],
        &guidBuffer[8],&guidBuffer[9],&guidBuffer[10],&guidBuffer[11],&guidBuffer[12],&guidBuffer[13],&guidBuffer[14],&guidBuffer[15]);
    memcpy(&jackSubType, guidBuffer, 16);

    hr = pPropertyStore->GetValue(PKEY_DeviceClass_IconPath, &pv);
    if (hr != S_OK)
        return false;
    iconPath = QString::fromUtf16(reinterpret_cast<const ushort *>(pv.piVal));

    return true;
}

QPixmap QnWinAudioDeviceInfoPrivate::getDeviceIcon() {
    QStringList params = iconPath.split(QLatin1Char(','));
    if (params.size() < 2)
        return QPixmap();

    int percent1 = params[0].indexOf(QLatin1Char('%'));
    while (percent1 >= 0) {
        int percent2 = params[0].indexOf(QLatin1Char('%'), percent1+1);
        if (percent2 > percent1) {
            QString metaVal = params[0].mid(percent1, percent2-percent1+1);
            QString val = QString::fromLocal8Bit(qgetenv(metaVal.mid(1, metaVal.length()-2).toLatin1().constData()));
            params[0].replace(metaVal, val);
        }
        percent1 = params[0].indexOf(QLatin1Char('%'));
    }

    HMODULE library = LoadLibrary((LPCWSTR) params[0].constData());
    if (!library)
        return QPixmap();

    int resNumber = params[1].toInt();
    if (resNumber < 0) {
        resNumber = -resNumber;
    } else {
        iconGroupIndex = resNumber;
        iconGroupNum = 0;
        EnumResourceNames(library, RT_GROUP_ICON, enumFunc, (LONG_PTR) this);
        resNumber = iconGroupNum;
    }

    HICON hIcon = LoadIcon(library, MAKEINTRESOURCE(resNumber));
    QPixmap result;
    if (hIcon)
        result = qt_pixmapFromWinHICON(hIcon);
    DestroyIcon(hIcon);
    return result;
}

BOOL CALLBACK enumFunc(HMODULE /*hModule*/, LPCTSTR /*lpszType*/, LPTSTR lpszName,LONG_PTR lParam)
{
    QnWinAudioDeviceInfoPrivate* info = (QnWinAudioDeviceInfoPrivate *) lParam;
    if (info->iconGroupIndex == 0)
    {
        info->iconGroupNum = (short) lpszName;
        return false;
    }
    info->iconGroupIndex--;
    return true;
}

bool QnWinAudioDeviceInfoPrivate::getIsMicrophone() const {
    const GUID MicrophoneGUIDS[] = {
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
        if (MicrophoneGUIDS[i] == jackSubType)
            return true;

    // device type unknown. try to check device name
    return fullName.toLower().contains(lit("microphone"));
}


// -------------------------------------------------------------------------- //
// QnWinAudioDeviceInfo
// -------------------------------------------------------------------------- //
QnWinAudioDeviceInfo::QnWinAudioDeviceInfo(const QString &deviceName):
    d(new QnWinAudioDeviceInfoPrivate())
{
    d->init(deviceName);
}

QnWinAudioDeviceInfo::~QnWinAudioDeviceInfo() {
    d->deinit();
}

QString QnWinAudioDeviceInfo::fullName() const {
    return d->fullName;
}

QPixmap QnWinAudioDeviceInfo::deviceIcon() const {
    return d->getDeviceIcon();
}

bool QnWinAudioDeviceInfo::isMicrophone() const {
    return d->getIsMicrophone();
}

#endif // Q_OS_WIN
