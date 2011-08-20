#include "recordingsettingswidget.h"
#include "ui_recordingsettings.h"

#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#include "QtMultimedia/QAudioDeviceInfo"

#ifdef _WIN32
#include "../device_plugins/desktop/win_audio_helper.h"
#endif

static const int ICON_SIZE = 32;

void RecordingSettingsWidget::setDefaultSoundIcon(QLabel* l)
{
    l->setPixmap(QPixmap(QLatin1String(":/skin/microphone.png")).scaled(ICON_SIZE, ICON_SIZE));
}

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
                                        arg(geometry.height()), i);
        } else {
            ui->screenComboBox->addItem(tr("Screen %1 - %2x%3").
                                        arg(i + 1).
                                        arg(geometry.width()).
                                        arg(geometry.height()), i);
        }
    }
    setScreen(settings->screen());

    connect (ui->primaryAudioDeviceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxChanged(int)) );
    connect (ui->secondaryAudioDeviceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxChanged(int)) );
    connect(ui->screenComboBox, SIGNAL(currentIndexChanged(int)), SLOT(onMonitorChanged(int)));
    setDefaultSoundIcon(ui->label_primaryDeviceIcon);
    setDefaultSoundIcon(ui->label_secondaryDeviceIcon);

    foreach (const QAudioDeviceInfo &info, QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
    {
#ifdef Q_OS_WIN
        WinAudioExtendInfo ext1(info.deviceName());
        ui->primaryAudioDeviceComboBox->addItem(ext1.fullName());
        ui->secondaryAudioDeviceComboBox->addItem(ext1.fullName());
#else
        ui->primaryAudioDeviceComboBox->addItem(info.deviceName());
        ui->secondaryAudioDeviceComboBox->addItem(info.deviceName());
#endif
    }
    setPrimaryAudioDeviceName(settings->primaryAudioDevice().deviceName());
    setSecondaryAudioDeviceName(settings->secondaryAudioDevice().deviceName());

    ui->captureCursorCheckBox->setChecked(settings->captureCursor());

#ifdef Q_OS_WIN
    ui->disableAeroCheckBox->setEnabled(false);
    typedef HRESULT (WINAPI *DwmIsCompositionEnabled)(BOOL*);
    QLibrary lib(QLatin1String("Dwmapi"));
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
                                    arg(geometry.height()), screen);
    } else {
        ui->disableAeroCheckBox->setEnabled(true);
    }
#endif
}

void RecordingSettingsWidget::onComboboxChanged(int index)
{
    additionalAdjustSize();
#ifdef _WIN32
    QComboBox* c = (QComboBox*) sender();
    WinAudioExtendInfo info(c->itemText(index));
    QLabel* l = c == ui->primaryAudioDeviceComboBox ? ui->label_primaryDeviceIcon : ui->label_secondaryDeviceIcon;
    QPixmap icon = info.deviceIcon();
    if (!icon.isNull())
        l->setPixmap(icon.scaled(ICON_SIZE, ICON_SIZE));
    else
        setDefaultSoundIcon(l);
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
        QMessageBox::information(this, tr("Information"), tr("Very powerful machine is required for BestQuality and Native resolution."));
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
    return ui->screenComboBox->itemData(ui->screenComboBox->currentIndex()).toInt();
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
    if (name.isEmpty()) {
        ui->primaryAudioDeviceComboBox->setCurrentIndex(0);
        return;
    }

    for (int i = 1; i < ui->primaryAudioDeviceComboBox->count(); i++) {
        if (ui->primaryAudioDeviceComboBox->itemText(i).startsWith(name)) {
            ui->primaryAudioDeviceComboBox->setCurrentIndex(i);
            return;
        }
    }
}

QString RecordingSettingsWidget::secondaryAudioDeviceName() const
{
    return ui->secondaryAudioDeviceComboBox->currentText();
}

void RecordingSettingsWidget::setSecondaryAudioDeviceName(const QString &name)
{
    if (name.isEmpty()) {
        ui->secondaryAudioDeviceComboBox->setCurrentIndex(0);
        return;
    }

    for (int i = 1; i < ui->secondaryAudioDeviceComboBox->count(); i++) {
        if (ui->secondaryAudioDeviceComboBox->itemText(i).startsWith(name)) {
            ui->secondaryAudioDeviceComboBox->setCurrentIndex(i);
            return;
        }
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
                                    arg(geometry.height()), screen);
    } else {
        ui->screenComboBox->clear();
        for (int i = 0; i < desktop->screenCount(); i++) {
           QRect geometry = desktop->screenGeometry(i);
           if (i == desktop->primaryScreen()) {
               ui->screenComboBox->addItem(tr("Screen %1 - %2x%3 (Primary)").
                                        arg(i + 1).
                                        arg(geometry.width()).
                                        arg(geometry.height()), i);
            } else {
                ui->screenComboBox->addItem(tr("Screen %1 - %2x%3").
                                        arg(i + 1).
                                        arg(geometry.width()).
                                        arg(geometry.height()), i);
            }
        }
    }
}

void RecordingSettingsWidget::additionalAdjustSize()
{
    ui->label_primaryDeviceText->adjustSize();
    ui->label_secondaryDeviceText->adjustSize();

    int maxWidth = qMax(ui->label_primaryDeviceText->width(), ui->label_secondaryDeviceText->width());
    QSize s = ui->label_primaryDeviceText->minimumSize();
    s.setWidth(maxWidth);
    ui->label_primaryDeviceText->setMinimumSize(s);
    ui->label_secondaryDeviceText->setMinimumSize(s);

    ui->label_primaryDeviceText->adjustSize();
    ui->label_secondaryDeviceText->adjustSize();
}
