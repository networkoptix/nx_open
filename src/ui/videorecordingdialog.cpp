#include "videorecordingdialog.h"
#include "ui_videorecordingdialog.h"

#include <QtGui/QFileDialog>

VideoRecordingDialog::VideoRecordingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VideoRecordingDialog)
{
    ui->setupUi(this);
#ifndef Q_OS_WIN
    ui->fullscreenNoAeroButton->setVisible(false);
#elif
    ui->fullscreenNoAeroButton->setEnabled(true);
#endif
}

VideoRecordingDialog::~VideoRecordingDialog()
{
    delete ui;
}

VideoRecordingDialog::CaptureMode VideoRecordingDialog::captureMode() const
{
    if (ui->fullscreenButton->isChecked())
        return FullScreenMode;
    else if (ui->fullscreenNoAeroButton->isChecked())
        return FullScreenNoeroMode;
    else
        return WindowMode;
}

void VideoRecordingDialog::setCaptureMode(VideoRecordingDialog::CaptureMode c)
{
    switch (c) {
    case FullScreenMode:
        ui->fullscreenButton->setChecked(true);
        break;
    case FullScreenNoeroMode:
        ui->fullscreenNoAeroButton->setChecked(true);
        break;
    case WindowMode:
        ui->windowButton->setChecked(true);
        break;
    default:
        break;
    }
}

VideoRecordingDialog::DecoderQuality VideoRecordingDialog::decoderQuality() const
{
    return (DecoderQuality)ui->qualityComboBox->currentIndex();
}

void VideoRecordingDialog::setDecoderQuality(VideoRecordingDialog::DecoderQuality q)
{
    ui->qualityComboBox->setCurrentIndex(q);
}

VideoRecordingDialog::Resolution VideoRecordingDialog::resolution() const
{
    return (Resolution)ui->resolutionComboBox->currentIndex();
}

void VideoRecordingDialog::setResolution(VideoRecordingDialog::Resolution r)
{
    ui->resolutionComboBox->setCurrentIndex(r);
}
