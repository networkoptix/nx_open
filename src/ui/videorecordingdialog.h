#ifndef VIDEORECORDINGDIALOG_H
#define VIDEORECORDINGDIALOG_H

#include <QtGui/QDialog>

namespace Ui {
    class VideoRecordingDialog;
}

class VideoRecordingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoRecordingDialog(QWidget *parent = 0);
    ~VideoRecordingDialog();

    enum CaptureMode { FullScreenMode, FullScreenNoeroMode, WindowMode };
    enum DecoderQuality { BestQuality, BalancedQuality, PerformanceQuality };
    enum Resolution { ResNative, ResQuaterNative, Res1920x1080, Res1280x720, Res640x480 };
    Q_ENUMS(CaptureMode)
    Q_ENUMS(DecoderQuality)
    Q_ENUMS(Resolution)

    CaptureMode captureMode() const;
    void setCaptureMode(CaptureMode c);
    
    DecoderQuality decoderQuality() const;
    void setDecoderQuality(DecoderQuality q);
    
    Resolution resolution() const;
    void setResolution(Resolution r);

    int screen() const;
    void setScreen(int screen);

    QString audioDeviceName() const;
    void setAudioDeviceName(const QString &name);

public slots:
    void accept();

private:
    Ui::VideoRecordingDialog *ui;
};

#endif // VIDEORECORDINGDIALOG_H
