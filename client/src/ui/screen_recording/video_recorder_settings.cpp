#include "video_recorder_settings.h"

#include <QtCore/QSettings>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtMultimedia/QAudioDeviceInfo>

#ifdef Q_OS_WIN
#   include <d3d9.h>
#   include <plugins/resource/desktop_win/screen_grabber.h>
#   include <plugins/resource/desktop_win/desktop_file_encoder.h>
#   include <plugins/resource/desktop_win/win_audio_device_info.h>
#endif

#include <utils/common/warnings.h>

QRegExp QnVideoRecorderSettings::m_devNumberExpr(QLatin1String(" \\([0-9]+\\)$"));

QnVideoRecorderSettings::QnVideoRecorderSettings(QObject *parent) :
    QObject(parent)
{
    settings.beginGroup(QLatin1String("videoRecording"));
    
    /*
    // update settings from previous version
    QString primary = settings.value(QLatin1String("primaryAudioDevice")).toString();
    QString secondary = settings.value(QLatin1String("secondaryAudioDevice")).toString();
    if (primary == secondary && !primary.isEmpty() && primary != QLatin1String("default") && !primary.contains(m_devNumberExpr))
    {
        //primary   += QLatin1String(" (1)");
        secondary += QLatin1String(" (2)");
        QStringList devices = availableDeviceNames(QAudio::AudioInput);
        for (int i = 0; i < devices.size(); ++i) 
        {
            if (devices[i] == secondary) {
                settings.setValue(QLatin1String("primaryAudioDevice"), primary);
                settings.setValue(QLatin1String("secondaryAudioDevice"), secondary);
            }
        }
    }
    */
}

QnVideoRecorderSettings::~QnVideoRecorderSettings()
{
    settings.endGroup();
}

QString QnVideoRecorderSettings::getFullDeviceName(const QString& shortName)
{
#ifdef Q_OS_WIN
    return QnWinAudioDeviceInfo(shortName).fullName();
#else
    return shortName;
#endif
}

QStringList QnVideoRecorderSettings::availableDeviceNames(QAudio::Mode mode)
{
    QMap<QString, int> devices;
    foreach (const QAudioDeviceInfo &info, QAudioDeviceInfo::availableDevices(mode)) 
    {
        QString devName = getFullDeviceName(info.deviceName());
        QMap<QString, int>::iterator existingDevice = devices.find(devName);
        if (existingDevice == devices.end())
            devices.insert(devName, 1);
        else
            existingDevice.value()++;
    }

    QStringList result;
    for(QMap<QString, int>::iterator itr = devices.begin(); itr != devices.end(); ++itr)
    {
        result << itr.key();
        for (int i = 2; i <= itr.value(); ++i)
            result << (itr.key() + QString(QLatin1String(" (%1)")).arg(i));
    }
    return result;
}

void QnVideoRecorderSettings::splitFullName(const QString& name, QString& shortName, int& index)
{
    shortName = name;
    int sameNamePos = name.lastIndexOf(m_devNumberExpr);
    index = 1;
    if (sameNamePos > 0) {
        index = name.mid(sameNamePos+2, name.lastIndexOf(QLatin1String(")")) - sameNamePos - 2).toInt();
        shortName = name.left(sameNamePos);
    }
}


QnAudioDeviceInfo QnVideoRecorderSettings::getDeviceByName(const QString& _name, QAudio::Mode mode, bool *isDefault) const
{
    QString name;
    int devNum = 1;
    splitFullName(_name, name, devNum);

    if (isDefault)
        *isDefault = false;
    if (name == QObject::tr("None"))
        return QnAudioDeviceInfo();

    foreach (const QAudioDeviceInfo &info, QAudioDeviceInfo::availableDevices(mode)) 
    {
        if (name.startsWith(info.deviceName())) {
            if (--devNum == 0)
                return QnAudioDeviceInfo(info, _name);
        }
    }

    if (isDefault)
        *isDefault = true;
    return QnAudioDeviceInfo(QAudioDeviceInfo::defaultInputDevice(), QString());
}

QnAudioDeviceInfo QnVideoRecorderSettings::primaryAudioDevice() const
{
    return getDeviceByName(settings.value(QLatin1String("primaryAudioDevice")).toString(), QAudio::AudioInput);
}

QString QnVideoRecorderSettings::primaryAudioDeviceName() const
{
    return settings.value(QLatin1String("primaryAudioDevice")).toString();
}

void QnVideoRecorderSettings::setPrimaryAudioDeviceByName(const QString &audioDeviceName)
{
    settings.setValue(QLatin1String("primaryAudioDevice"), audioDeviceName);
}

QnAudioDeviceInfo QnVideoRecorderSettings::secondaryAudioDevice() const
{
    QnAudioDeviceInfo result;
    bool isDefault = true;
    if (isDefault)
        result = getDeviceByName(QLatin1String("Rec. Playback"), QAudio::AudioInput, &isDefault);
    if (isDefault)
        result = getDeviceByName(QLatin1String("Stereo mix"), QAudio::AudioInput, &isDefault);
    if (isDefault)
        result = getDeviceByName(QLatin1String("What you hear"), QAudio::AudioInput, &isDefault);
    if (!isDefault)
        return result;

    return getDeviceByName(settings.value(QLatin1String("secondaryAudioDevice")).toString(), QAudio::AudioInput);
}


QString QnVideoRecorderSettings::secondaryAudioDeviceName() const
{
    return settings.value(QLatin1String("secondaryAudioDevice")).toString();
}


void QnVideoRecorderSettings::setSecondaryAudioDeviceByName(const QString &audioDeviceName)
{
    settings.setValue(QLatin1String("secondaryAudioDevice"), audioDeviceName);
}

bool QnVideoRecorderSettings::captureCursor() const
{
    if (!settings.contains(QLatin1String("captureCursor")))
        return true;
    return settings.value(QLatin1String("captureCursor")).toBool();
}

void QnVideoRecorderSettings::setCaptureCursor(bool yes)
{
    settings.setValue(QLatin1String("captureCursor"), yes);
}

Qn::CaptureMode QnVideoRecorderSettings::captureMode() const
{
    return (Qn::CaptureMode) settings.value(QLatin1String("captureMode")).toInt();
}

void QnVideoRecorderSettings::setCaptureMode(Qn::CaptureMode captureMode)
{
    settings.setValue(QLatin1String("captureMode"), captureMode);
}

Qn::DecoderQuality QnVideoRecorderSettings::decoderQuality() const
{
    if (!settings.contains(QLatin1String("decoderQuality")))
        return Qn::BalancedQuality;
    return (Qn::DecoderQuality) settings.value(QLatin1String("decoderQuality")).toInt();
}

void QnVideoRecorderSettings::setDecoderQuality(Qn::DecoderQuality decoderQuality)
{
    settings.setValue(QLatin1String("decoderQuality"), decoderQuality);
}

Qn::Resolution QnVideoRecorderSettings::resolution() const {
    Qn::Resolution rez = Qn::QuaterNativeResolution;
    if (settings.contains(lit("resolution")))
        rez = (Qn::Resolution) settings.value(lit("resolution")).toInt();
    return rez;
}

void QnVideoRecorderSettings::setResolution(Qn::Resolution resolution) {
    settings.setValue(lit("resolution"), resolution);
}

int QnVideoRecorderSettings::screen() const
{
    int oldScreen = settings.value(QLatin1String("screen")).toInt();
    QRect geometry = settings.value(QLatin1String("screenResolution")).toRect();
    QDesktopWidget *desktop = qApp->desktop();
    if (desktop->screen(oldScreen)->geometry() == geometry) {
        return oldScreen;
    } else {
        for (int i = 0; i < desktop->screenCount(); i++) {
            if (desktop->screen(i)->geometry() == geometry)
                return i;
        }
    }
    return desktop->primaryScreen();
}

void QnVideoRecorderSettings::setScreen(int screen)
{
    settings.setValue(QLatin1String("screen"), screen);
    settings.setValue(QLatin1String("screenResolution"),  qApp->desktop()->screen(screen)->geometry());
}

QString QnVideoRecorderSettings::recordingFolder() const {
    if (!settings.contains(QLatin1String("recordingFolder")))
        return QDir::tempPath();
    return settings.value(QLatin1String("recordingFolder")).toString();
}

void QnVideoRecorderSettings::setRecordingFolder(QString folder) {
    settings.setValue(QLatin1String("recordingFolder"), folder);
}

float QnVideoRecorderSettings::qualityToNumeric(Qn::DecoderQuality quality) 
{
    switch(quality) {
        case Qn::BestQuality:        return 1.0;
        case Qn::BalancedQuality:    return 0.75;
        case Qn::PerformanceQuality: return 0.5;
        default:
            qnWarning("Invalid quality value '%1', treating as best quality.", static_cast<int>(quality));
            return 1.0;
    }
}

int QnVideoRecorderSettings::screenToAdapter(int screen)
{
#ifdef Q_OS_WIN
    IDirect3D9* pD3D;
    if((pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return 0;

    QRect rect = qApp->desktop()->screenGeometry(screen);
    MONITORINFO monInfo;
    memset(&monInfo, 0, sizeof(monInfo));
    monInfo.cbSize = sizeof(monInfo);
    int rez = 0;

    for (int i = 0; i < qApp->desktop()->screenCount(); ++i) {
        if (!GetMonitorInfo(pD3D->GetAdapterMonitor(i), &monInfo))
            break;
        if (monInfo.rcMonitor.left == rect.left() && monInfo.rcMonitor.top == rect.top()) {
            rez = i;
            break;
        }
    }
    pD3D->Release();
    return rez;
#else
    return screen;
#endif
}

QSize QnVideoRecorderSettings::resolutionToSize(Qn::Resolution resolution) 
{
    QSize result(0, 0);
    switch(resolution) {
        case Qn::NativeResolution:          return QSize(0, 0);
        case Qn::QuaterNativeResolution:    return QSize(-2, -2);
        case Qn::Exact1920x1080Resolution:  return QSize(1920, 1080);
        case Qn::Exact1280x720Resolution:   return QSize(1280, 720);
        case Qn::Exact640x480Resolution:    return QSize(640, 480);
        case Qn::Exact320x240Resolution:    return QSize(320, 240);
        default:
            qnWarning("Invalid resolution value '%1', treating as native resolution.", static_cast<int>(resolution));
            return QSize(0, 0);
    }
}

