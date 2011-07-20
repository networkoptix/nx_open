#include "videorecordingdialog.h"
#include "ui_videorecordingdialog.h"

#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#include "QtMultimedia/QAudioDeviceInfo"

VideoRecordingDialog::VideoRecordingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VideoRecordingDialog),
    settings(new VideoRecorderSettings(this))
{
    ui->setupUi(this);
#ifndef Q_OS_WIN
    ui->fullscreenNoAeroButton->setVisible(false);
#else
    ui->fullscreenNoAeroButton->setEnabled(true);
#endif

    setCaptureMode(settings->captureMode());
    setDecoderQuality(settings->decoderQuality());
    setResolution(settings->resolution());

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
    setScreen(settings->screen());

    foreach (const QAudioDeviceInfo &info, QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        ui->audioDevicesComboBox->addItem(info.deviceName());
    }
    setAudioDeviceName(settings->audioDevice().deviceName());
}

VideoRecordingDialog::~VideoRecordingDialog()
{
    delete ui;
}

VideoRecorderSettings::CaptureMode VideoRecordingDialog::captureMode() const
{
    if (ui->fullscreenButton->isChecked())
        return VideoRecorderSettings::FullScreenMode;
    else if (ui->fullscreenNoAeroButton->isChecked())
        return VideoRecorderSettings::FullScreenNoeroMode;
    else
        return VideoRecorderSettings::WindowMode;
}

void VideoRecordingDialog::setCaptureMode(VideoRecorderSettings::CaptureMode c)
{
    switch (c) {
    case VideoRecorderSettings::FullScreenMode:
        ui->fullscreenButton->setChecked(true);
        break;
    case VideoRecorderSettings::FullScreenNoeroMode:
        ui->fullscreenNoAeroButton->setChecked(true);
        break;
    case VideoRecorderSettings::WindowMode:
        ui->windowButton->setChecked(true);
        break;
    default:
        break;
    }
}

VideoRecorderSettings::DecoderQuality VideoRecordingDialog::decoderQuality() const
{
    return (VideoRecorderSettings::DecoderQuality)ui->qualityComboBox->currentIndex();
}

void VideoRecordingDialog::setDecoderQuality(VideoRecorderSettings::DecoderQuality q)
{
    ui->qualityComboBox->setCurrentIndex(q);
}

VideoRecorderSettings::Resolution VideoRecordingDialog::resolution() const
{
    return (VideoRecorderSettings::Resolution)ui->resolutionComboBox->currentIndex();
}

void VideoRecordingDialog::setResolution(VideoRecorderSettings::Resolution r)
{
    ui->resolutionComboBox->setCurrentIndex(r);
}

void VideoRecordingDialog::accept()
{
    settings->setCaptureMode(captureMode());
    settings->setDecoderQuality(decoderQuality());
    settings->setResolution(resolution());
    settings->setScreen(screen());
    settings->setAudioDeviceByName(audioDeviceName());

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

QString VideoRecordingDialog::audioDeviceName() const
{
    return ui->audioDevicesComboBox->currentText();
}

void VideoRecordingDialog::setAudioDeviceName(const QString &name)
{
    for (int i = 0; i < ui->audioDevicesComboBox->count(); i++) {
        if (ui->audioDevicesComboBox->itemText(i) == name)
            ui->audioDevicesComboBox->setCurrentIndex(i);
    }
}
