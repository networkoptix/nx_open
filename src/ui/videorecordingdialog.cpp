#include "videorecordingdialog.h"
#include "ui_videorecordingdialog.h"


#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>

VideoRecordingDialog::VideoRecordingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VideoRecordingDialog)
{
    ui->setupUi(this);
#ifndef Q_OS_WIN
    ui->fullscreenNoAeroButton->setVisible(false);
#else
    ui->fullscreenNoAeroButton->setEnabled(true);
#endif
    QSettings settings;
    settings.beginGroup("videoRecording");

    setCaptureMode((VideoRecordingDialog::CaptureMode)settings.value("captureMode").toInt());
    setDecoderQuality((VideoRecordingDialog::DecoderQuality)settings.value("decoderQuality").toInt());
    setResolution((VideoRecordingDialog::Resolution)settings.value("resolution").toInt());

    QDesktopWidget *desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); i++) {
        QRect geometry = desktop->screenGeometry(i);
        if (i == desktop->primaryScreen()) {
            ui->screenComboBox->addItem(tr("Screen %1 - %2x%3 (Primary)").
                                        arg(i + 1).
                                        arg(geometry.width()).
                                        arg(geometry.height()));
        } else {
            ui->screenComboBox->addItem(tr("Screen %1 - %2x%3").
                                        arg(i + 1).
                                        arg(geometry.width()).
                                        arg(geometry.height()));
        }
    }
    setScreen(settings.value("screen").toInt());

    settings.endGroup();
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

void VideoRecordingDialog::accept()
{
    QSettings settings;
    settings.beginGroup("videoRecording");

    settings.setValue("captureMode", captureMode());
    settings.setValue("decoderQuality", decoderQuality());
    settings.setValue("resolution", resolution());
    settings.setValue("screen", screen());

    settings.endGroup();

    QDialog::accept();
}

int VideoRecordingDialog::screen() const
{
    return ui->screenComboBox->currentIndex();
}

void VideoRecordingDialog::setScreen(int screen)
{
    ui->screenComboBox->setCurrentIndex(screen);
}
