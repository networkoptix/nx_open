#include "videorecordersettings.h"

#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>

VideoRecorderSettings::VideoRecorderSettings(QObject *parent) :
    QObject(parent)
{
    settings.beginGroup(QLatin1String("videoRecording"));
}

VideoRecorderSettings::~VideoRecorderSettings()
{
    settings.endGroup();
}

QAudioDeviceInfo getDeviceByName(const QString &name, QAudio::Mode mode)
{
    foreach (const QAudioDeviceInfo &info, QAudioDeviceInfo::availableDevices(mode)) {
        if (info.deviceName() == name)
            return info;
    }
    return QAudioDeviceInfo::defaultInputDevice();
}

QAudioDeviceInfo VideoRecorderSettings::audioDevice() const
{
    return getDeviceByName(settings.value(QLatin1String("audioDevice")).toString(), QAudio::AudioInput);
}

void VideoRecorderSettings::setAudioDeviceByName(const QString &audioDeviceName)
{
    settings.setValue(QLatin1String("audioDevice"), audioDeviceName);
}

VideoRecorderSettings::CaptureMode VideoRecorderSettings::captureMode() const
{
    return (VideoRecorderSettings::CaptureMode)settings.value(QLatin1String("captureMode")).toInt();
}

void VideoRecorderSettings::setCaptureMode(VideoRecorderSettings::CaptureMode captureMode)
{
    settings.setValue(QLatin1String("captureMode"), captureMode);
}

VideoRecorderSettings::DecoderQuality VideoRecorderSettings::decoderQuality() const
{
    return (VideoRecorderSettings::DecoderQuality)settings.value(QLatin1String("decoderQuality")).toInt();
}

void VideoRecorderSettings::setDecoderQuality(VideoRecorderSettings::DecoderQuality decoderQuality)
{
    settings.setValue(QLatin1String("decoderQuality"), decoderQuality);
}

VideoRecorderSettings::Resolution VideoRecorderSettings::resolution() const
{
    return (VideoRecorderSettings::Resolution)settings.value(QLatin1String("resolution")).toInt();
}

void VideoRecorderSettings::setResolution(VideoRecorderSettings::Resolution resolution)
{
    settings.setValue(QLatin1String("resolution"), resolution);
}

int VideoRecorderSettings::screen() const
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

void VideoRecorderSettings::setScreen(int screen)
{
    settings.setValue(QLatin1String("screen"), screen);
    settings.setValue(QLatin1String("screenResolution"),  qApp->desktop()->screen(screen)->geometry());
}
