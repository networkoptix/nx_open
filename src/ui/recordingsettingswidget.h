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

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceName(const QString &name);

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceName(const QString &name);

public slots:
    void accept();

private slots:
    void onMonitorChanged(int);

private:
    Ui::RecordingSettings *ui;
    VideoRecorderSettings *settings;
};

#endif // VIDEORECORDINGDIALOG_H
