#ifndef VIDEORECORDINGDIALOG_H
#define VIDEORECORDINGDIALOG_H

#include <QtGui/QWidget>
#include "videorecordersettings.h"

namespace Ui {
    class RecordingSettings;
}

class VideoRecorderSettings;
class RecordingSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingSettingsWidget(QWidget *parent = 0);
    ~RecordingSettingsWidget();

    VideoRecorderSettings::CaptureMode captureMode() const;
    void setCaptureMode(VideoRecorderSettings::CaptureMode c);
    
    VideoRecorderSettings::DecoderQuality decoderQuality() const;
    void setDecoderQuality(VideoRecorderSettings::DecoderQuality q);
    
    VideoRecorderSettings::Resolution resolution() const;
    void setResolution(VideoRecorderSettings::Resolution r);

    int screen() const;
    void setScreen(int screen);

    QString audioDeviceName() const;
    void setAudioDeviceName(const QString &name);

public slots:
    void accept();

private:
    Ui::RecordingSettings *ui;
    VideoRecorderSettings *settings;
};

#endif // VIDEORECORDINGDIALOG_H
