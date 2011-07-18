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
    enum Resolution { ResNative, ResHalfNative, Res1920x1080, Res1280x720, Res640x480 };
    Q_ENUMS(CaptureMode)
    Q_ENUMS(DecoderQuality)
    Q_ENUMS(Resolution)

    CaptureMode captureMode() const;
    DecoderQuality decoderQuality() const;
    Resolution resolution() const;

private slots:
    void onSaveAsPressed();
    void onStartRecordingPressed();

private:
    Ui::VideoRecordingDialog *ui;
};

#endif // VIDEORECORDINGDIALOG_H
