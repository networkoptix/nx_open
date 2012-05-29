#ifndef VIDEORECORDINGDIALOG_H
#define VIDEORECORDINGDIALOG_H

#include <QWidget>

#include "ui/screen_recording/video_recorder_settings.h"

namespace Ui {
    class RecordingSettings;
}

class VideoRecorderSettings;
class QnRecordingSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnRecordingSettingsWidget(QWidget *parent = 0);
    ~QnRecordingSettingsWidget();

    VideoRecorderSettings::CaptureMode captureMode() const;
    void setCaptureMode(VideoRecorderSettings::CaptureMode c);

    VideoRecorderSettings::DecoderQuality decoderQuality() const;
    void setDecoderQuality(VideoRecorderSettings::DecoderQuality q);

    VideoRecorderSettings::Resolution resolution() const;
    void setResolution(VideoRecorderSettings::Resolution r);

    int screen() const;
    void setScreen(int screen);

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceName(const QString &name);

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceName(const QString &name);

public slots:
    void accept();
    void onComboboxChanged(int index);
private:
    void additionalAdjustSize();
private slots:
    void onMonitorChanged(int);
    void onDisableAeroChecked(bool);

private:
    Ui::RecordingSettings *ui;
    VideoRecorderSettings *settings;
};

#endif // VIDEORECORDINGDIALOG_H
