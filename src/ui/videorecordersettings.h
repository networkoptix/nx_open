#ifndef VIDEORECORDERSETTINGS_H
#define VIDEORECORDERSETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtMultimedia/QAudioDeviceInfo>

class VideoRecorderSettings : public QObject
{
    Q_OBJECT
public:
    explicit VideoRecorderSettings(QObject *parent = 0);
    ~VideoRecorderSettings();

    enum CaptureMode { FullScreenMode, FullScreenNoeroMode, WindowMode };
    enum DecoderQuality { BestQuality, BalancedQuality, PerformanceQuality };
    enum Resolution { ResNative, ResQuaterNative, Res1920x1080, Res1280x720, Res640x480 };
    Q_ENUMS(CaptureMode)
    Q_ENUMS(DecoderQuality)
    Q_ENUMS(Resolution)

    QAudioDeviceInfo primaryAudioDevice() const;
    void setPrimaryAudioDeviceByName(const QString &name);

    QAudioDeviceInfo secondaryAudioDevice() const;
    void setSecondaryAudioDeviceByName(const QString &name);

    bool captureCursor() const;
    void setCaptureCursor(bool yes);

    CaptureMode captureMode() const;
    void setCaptureMode(CaptureMode c);

    DecoderQuality decoderQuality() const;
    void setDecoderQuality(DecoderQuality q);

    Resolution resolution() const;
    void setResolution(Resolution r);

    int screen() const;
    void setScreen(int screen);

private:
    QSettings settings;
};

#endif // VIDEORECORDERSETTINGS_H
