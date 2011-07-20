#ifndef VIDEORECORDINGDIALOG_H
#define VIDEORECORDINGDIALOG_H

#include <QtGui/QDialog>
#include "videorecordersettings.h"

namespace Ui {
    class VideoRecordingDialog;
}

class VideoRecorderSettings;
class VideoRecordingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoRecordingDialog(QWidget *parent = 0);
    ~VideoRecordingDialog();

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
    Ui::VideoRecordingDialog *ui;
    VideoRecorderSettings *settings;
};

#endif // VIDEORECORDINGDIALOG_H
