#include "video_recorder_settings.h"

#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>

QnVideoRecorderSettings::QnVideoRecorderSettings(QObject *parent) :
    QObject(parent)
{
    settings.beginGroup(QLatin1String("videoRecording"));
}

QnVideoRecorderSettings::~QnVideoRecorderSettings()
{
    settings.endGroup();
}

QAudioDeviceInfo getDeviceByName(const QString &name, QAudio::Mode mode, bool *isDefault = 0)
{
    if (isDefault)
        *isDefault = false;
    if (name == QObject::tr("None"))
        return QAudioDeviceInfo();

    foreach (const QAudioDeviceInfo &info, QAudioDeviceInfo::availableDevices(mode)) {
        if (name.startsWith(info.deviceName()))
            return info;
    }

    if (isDefault)
        *isDefault = true;
    return QAudioDeviceInfo::defaultInputDevice();
}

QAudioDeviceInfo QnVideoRecorderSettings::primaryAudioDevice() const
{
    return getDeviceByName(settings.value(QLatin1String("primaryAudioDevice")).toString(), QAudio::AudioInput);
}

void QnVideoRecorderSettings::setPrimaryAudioDeviceByName(const QString &audioDeviceName)
{
    settings.setValue(QLatin1String("primaryAudioDevice"), audioDeviceName);
}

QAudioDeviceInfo QnVideoRecorderSettings::secondaryAudioDevice() const
{
    QAudioDeviceInfo result;
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
