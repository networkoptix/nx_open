#ifndef VIDEORECORDERSETTINGS_H
#define VIDEORECORDERSETTINGS_H

#include <QtCore/QRegExp>
#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtMultimedia/QAudioDeviceInfo>
#include "qnaudio_device_info.h"

namespace Qn {
    
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

    QnAudioDeviceInfo primaryAudioDevice() const;

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceByName(const QString &name);

    QnAudioDeviceInfo secondaryAudioDevice() const;

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceByName(const QString &name);

    bool captureCursor() const;
    void setCaptureCursor(bool yes);

    Qn::DecoderQuality decoderQuality() const;
    void setDecoderQuality(Qn::DecoderQuality q);

    Qn::Resolution resolution() const;
    void setResolution(Qn::Resolution r);

    QString recordingFolder() const;
    void setRecordingFolder(QString folder);

    static QString getFullDeviceName(const QString& shortName);
    static QStringList availableDeviceNames(QAudio::Mode mode);
    static void splitFullName(const QString& name, QString& shortName, int& index);


    static int screenToAdapter(int screen);
    static QSize resolutionToSize(Qn::Resolution resolution);
    static float qualityToNumeric(Qn::DecoderQuality quality);
private:
    QnAudioDeviceInfo getDeviceByName(const QString &name, QAudio::Mode mode, bool *isDefault = 0) const;
private:
    QSettings settings;
    static QRegExp m_devNumberExpr;
};

#endif // VIDEORECORDERSETTINGS_H
