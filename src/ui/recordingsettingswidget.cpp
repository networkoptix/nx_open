#include "recordingsettingswidget.h"
#include "ui_recordingsettings.h"

#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#include "QtMultimedia/QAudioDeviceInfo"
#include "../device_plugins/desktop/win_audio_helper.h"

RecordingSettingsWidget::RecordingSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RecordingSettings),
    settings(new VideoRecorderSettings(this))
{
    ui->setupUi(this);
#ifndef Q_OS_WIN
    ui->disableAeroCheckBox->setVisible(false);
#else
    ui->disableAeroCheckBox->setEnabled(true);
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
        ui->primaryAudioDeviceComboBox->addItem(WinAudioExtendInfo(info.deviceName()).fullName());
        ui->secondaryAudioDeviceComboBox->addItem(WinAudioExtendInfo(info.deviceName()).fullName());
    }
    setPrimaryAudioDeviceName(settings->primaryAudioDevice().deviceName());
    setSecondaryAudioDeviceName(settings->secondaryAudioDevice().deviceName());

    ui->captureCursorCheckBox->setChecked(settings->captureCursor());

    connect(ui->screenComboBox, SIGNAL(currentIndexChanged(int)), SLOT(onMonitorChanged(int)));
    ui->label_primaryDeviceIcon->setPixmap(QPixmap(":/skin/sound.png").scaled(24, 24));
    ui->label_secondaryDeviceIcon->setPixmap(QPixmap(":/skin/microphone.png").scaled(24, 24));

#ifdef Q_OS_WIN
    ui->disableAeroCheckBox->setEnabled(false);
    typedef HRESULT (*DwmIsCompositionEnabled)(BOOL*);
    QLibrary lib("Dwmapi");
    bool ok = lib.load();
    if (!ok)
        return;

    BOOL enabled = true;
    DwmIsCompositionEnabled f = (DwmIsCompositionEnabled)lib.resolve("DwmIsCompositionEnabled");
    if (!f) {
        return;
    }
    
    f(&enabled);
    if (!enabled) {
        ui->screenComboBox->clear();
        int screen = desktop->primaryScreen();
        QRect geometry = desktop->screenGeometry(screen);
        ui->screenComboBox->addItem(tr("Screen %1 - %2x%3 (Primary)").
                                    arg(screen).
                                    arg(geometry.width()).
                                    arg(geometry.height()));
    } else {
        ui->disableAeroCheckBox->setEnabled(true);
    }
#endif
}

RecordingSettingsWidget::~RecordingSettingsWidget()
{
    delete ui;
}

VideoRecorderSettings::CaptureMode RecordingSettingsWidget::captureMode() const
{
    if (ui->fullscreenButton->isChecked() && !ui->disableAeroCheckBox->isChecked())
        return VideoRecorderSettings::FullScreenMode;
    else if (ui->fullscreenButton->isChecked() && ui->disableAeroCheckBox->isChecked())
        return VideoRecorderSettings::FullScreenNoeroMode;
    else
        return VideoRecorderSettings::WindowMode;
}

void RecordingSettingsWidget::setCaptureMode(VideoRecorderSettings::CaptureMode c)
{
    switch (c) {
    case VideoRecorderSettings::FullScreenMode:
        ui->fullscreenButton->setChecked(true);
        ui->disableAeroCheckBox->setChecked(false);
        break;
    case VideoRecorderSettings::FullScreenNoeroMode:
        ui->fullscreenButton->setChecked(true);
        ui->disableAeroCheckBox->setChecked(true);
        break;
    case VideoRecorderSettings::WindowMode:
        ui->windowButton->setChecked(true);
        break;
    default:
        break;
    }
}

VideoRecorderSettings::DecoderQuality RecordingSettingsWidget::decoderQuality() const
{
    return (VideoRecorderSettings::DecoderQuality)ui->qualityComboBox->currentIndex();
}

void RecordingSettingsWidget::setDecoderQuality(VideoRecorderSettings::DecoderQuality q)
{
    ui->qualityComboBox->setCurrentIndex(q);
}

VideoRecorderSettings::Resolution RecordingSettingsWidget::resolution() const
{
    return (VideoRecorderSettings::Resolution)ui->resolutionComboBox->currentIndex();
}

void RecordingSettingsWidget::setResolution(VideoRecorderSettings::Resolution r)
{
    ui->resolutionComboBox->setCurrentIndex(r);
}

void RecordingSettingsWidget::accept()
{
    settings->setCaptureMode(captureMode());
    settings->setDecoderQuality(decoderQuality());
    settings->setResolution(resolution());
    settings->setScreen(screen());
    settings->setPrimaryAudioDeviceByName(primaryAudioDeviceName());
    settings->setSecondaryAudioDeviceByName(secondaryAudioDeviceName());
    settings->setCaptureCursor(ui->captureCursorCheckBox->isChecked());

    if (decoderQuality() == VideoRecorderSettings::BestQuality && resolution() == VideoRecorderSettings::ResNative)
        QMessageBox::information(this, tr("Information"), tr("Very powerful machine is required for such settings"));
}

void RecordingSettingsWidget::onMonitorChanged(int index)
{
#ifdef Q_OS_WIN
    if (index != qApp->desktop()->primaryScreen())
    {
        if (ui->disableAeroCheckBox->isChecked())
            ui->fullscreenButton->setChecked(true);
        ui->disableAeroCheckBox->setEnabled(false);
    }
    else
    {
        ui->disableAeroCheckBox->setEnabled(true);
    }
#endif
}

int RecordingSettingsWidget::screen() const
{
    return ui->screenComboBox->currentIndex();
}

void RecordingSettingsWidget::setScreen(int screen)
{
    ui->screenComboBox->setCurrentIndex(screen);
}

QString RecordingSettingsWidget::primaryAudioDeviceName() const
{
    return ui->primaryAudioDeviceComboBox->currentText();
}


void RecordingSettingsWidget::setPrimaryAudioDeviceName(const QString &name)
{
    for (int i = 0; i < ui->primaryAudioDeviceComboBox->count(); i++) {
        if (ui->primaryAudioDeviceComboBox->itemText(i).startsWith(name))
            ui->primaryAudioDeviceComboBox->setCurrentIndex(i);
    }
}

QString RecordingSettingsWidget::secondaryAudioDeviceName() const
{
    return ui->secondaryAudioDeviceComboBox->currentText();
}

void RecordingSettingsWidget::setSecondaryAudioDeviceName(const QString &name)
{
    for (int i = 0; i < ui->secondaryAudioDeviceComboBox->count(); i++) {
        if (ui->secondaryAudioDeviceComboBox->itemText(i).startsWith(name))
            ui->secondaryAudioDeviceComboBox->setCurrentIndex(i);
    }
}

void RecordingSettingsWidget::onDisableAeroChecked(bool enabled)
{
    QDesktopWidget *desktop = qApp->desktop();
    if (enabled) {
        ui->screenComboBox->clear();
        int screen = desktop->primaryScreen();
        QRect geometry = desktop->screenGeometry(screen);
        ui->screenComboBox->addItem(tr("Screen %1 - %2x%3 (Primary)").
                                    arg(screen).
                                    arg(geometry.width()).
                                    arg(geometry.height()));
    } else {
        ui->screenComboBox->clear();
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
    }
}