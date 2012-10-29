#ifndef VIDEORECORDERSETTINGS_H
#define VIDEORECORDERSETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtMultimedia/QAudioDeviceInfo>

namespace Qn {
    enum CaptureMode { 
        FullScreenMode, 
        FullScreenNoeroMode, 
        WindowMode 
    };

    enum DecoderQuality { 
        BestQuality, 
        BalancedQuality, 
        PerformanceQuality 
    };

    enum Resolution { 
        NativeResolution, 
        QuaterNativeResolution, 
        Exact1920x1080Resolution, 
        Exact1280x720Resolution, 
        Exact640x480Resolution, 
        Exact320x240Resolution 
    };

} // namespace Qn

class QnVideoRecorderSettings : public QObject
{
    Q_OBJECT
public:
    explicit QnVideoRecorderSettings(QObject *parent = 0);
    ~QnVideoRecorderSettings();

    QAudioDeviceInfo primaryAudioDevice() const;
    void setPrimaryAudioDeviceByName(const QString &name);

    QAudioDeviceInfo secondaryAudioDevice() const;
    void setSecondaryAudioDeviceByName(const QString &name);

    bool captureCursor() const;
    void setCaptureCursor(bool yes);

    Qn::CaptureMode captureMode() const;
    void setCaptureMode(Qn::CaptureMode c);

    Qn::DecoderQuality decoderQuality() const;
    void setDecoderQuality(Qn::DecoderQuality q);

    Qn::Resolution resolution() const;
    void setResolution(Qn::Resolution r);

    int screen() const;
    void setScreen(int screen);

    QString recordingFolder() const;
    void setRecordingFolder(QString folder);

private:
    QSettings settings;
};

#endif // VIDEORECORDERSETTINGS_H
