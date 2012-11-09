#include "video_recorder_settings.h"

#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#ifdef Q_OS_WIN32
#   include "device_plugins/desktop/win_audio_helper.h"
#endif

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
    return WinAudioExtendInfo(shortName).fullName();
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

Qn::Resolution QnVideoRecorderSettings::resolution() const
{
    Qn::Resolution rez = Qn::QuaterNativeResolution;
    if (settings.contains(QLatin1String("resolution")))
        rez = (Qn::Resolution) settings.value(QLatin1String("resolution")).toInt();
#ifdef CL_TRIAL_MODE
    if ( rez < Qn::Exact640x480Resolution)
        rez = Qn::Exact640x480Resolution;
#endif
    return rez;
}

void QnVideoRecorderSettings::setResolution(Qn::Resolution resolution)
{
    settings.setValue(QLatin1String("resolution"), resolution);
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
