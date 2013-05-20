#include "recording_settings_widget.h"
#include "ui_recording_settings_widget.h"

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>

#include <QtMultimedia/QAudioDeviceInfo>

#include <ui/widgets/dwm.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#ifdef Q_OS_WIN
#   include "device_plugins/desktop_win/win_audio_helper.h"
#endif

namespace {
    const int ICON_SIZE = 32;

    void setDefaultSoundIcon(QLabel *label) {
        label->setPixmap(qnSkin->pixmap("microphone.png", QSize(ICON_SIZE, ICON_SIZE), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

} // anonymous namespace


QnRecordingSettingsWidget::QnRecordingSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RecordingSettings),
    m_settings(new QnVideoRecorderSettings(this)),
    m_dwm(new QnDwm(this))
{
    ui->setupUi(this);

#ifdef CL_TRIAL_MODE
    for (int i = 0; i < (int) QnVideoRecorderSettings::Res640x480; ++i)
        ui->resolutionComboBox->removeItem(0);
#endif

    QDesktopWidget *desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); i++) {
        bool isPrimaryScreen = (i == desktop->primaryScreen());
        if (!m_dwm->isSupported() && !isPrimaryScreen)
            continue; //TODO: #GDM can we record from secondary screen without DWM?

        QRect geometry = desktop->screenGeometry(i);
        QString item = tr("Screen %1 - %2x%3")
                .arg(i + 1)
                .arg(geometry.width())
                .arg(geometry.height());
        if (isPrimaryScreen)
            item = tr("%1 (Primary)").arg(item);
        ui->screenComboBox->addItem(item, i);
    }

    foreach (const QString& deviceName, QnVideoRecorderSettings::availableDeviceNames(QAudio::AudioInput)) {
        ui->primaryAudioDeviceComboBox->addItem(deviceName);
        ui->secondaryAudioDeviceComboBox->addItem(deviceName);
    }

    connect(ui->fullscreenButton,               SIGNAL(toggled(bool)),              ui->screenComboBox,         SLOT(setEnabled(bool)));
    connect(ui->fullscreenButton,               SIGNAL(toggled(bool)),              ui->disableAeroCheckBox,    SLOT(setEnabled(bool)));

    connect(ui->qualityComboBox,                SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateRecordingWarning()));
    connect(ui->resolutionComboBox,             SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateRecordingWarning()));
    connect(ui->primaryAudioDeviceComboBox,     SIGNAL(currentIndexChanged(int)),   this,   SLOT(onComboboxChanged(int)));
    connect(ui->secondaryAudioDeviceComboBox,   SIGNAL(currentIndexChanged(int)),   this,   SLOT(onComboboxChanged(int)));
    connect(ui->screenComboBox,                 SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateDisableAeroCheckbox()));
    connect(ui->browseRecordingFolderButton,    SIGNAL(clicked()),                  this,   SLOT(at_browseRecordingFolderButton_clicked()));
    connect(m_dwm,                              SIGNAL(compositionChanged(bool)),   this,   SLOT(at_dwm_compositionChanged(bool)));

    setWarningStyle(ui->recordingWarningLabel);
    setDefaultSoundIcon(ui->primaryDeviceIconLabel);
    setDefaultSoundIcon(ui->secondaryDeviceIconLabel);

    at_dwm_compositionChanged(m_dwm->isCompositionEnabled());
    updateDisableAeroCheckbox();
    updateRecordingWarning();
}

QnRecordingSettingsWidget::~QnRecordingSettingsWidget() {
    return;
}

void QnRecordingSettingsWidget::updateFromSettings() {
    setCaptureMode(m_settings->captureMode());
    setDecoderQuality(m_settings->decoderQuality());
    setResolution(m_settings->resolution());
    setScreen(m_settings->screen());
    setPrimaryAudioDeviceName(m_settings->primaryAudioDevice().fullName());
    setSecondaryAudioDeviceName(m_settings->secondaryAudioDevice().fullName());
    
    ui->captureCursorCheckBox->setChecked(m_settings->captureCursor());
    ui->recordingFolderLabel->setText(m_settings->recordingFolder());
}

void QnRecordingSettingsWidget::submitToSettings() {
    m_settings->setCaptureMode(captureMode());
    m_settings->setDecoderQuality(decoderQuality());
    m_settings->setResolution(resolution());
    m_settings->setScreen(screen());
    m_settings->setPrimaryAudioDeviceByName(primaryAudioDeviceName());
    m_settings->setSecondaryAudioDeviceByName(secondaryAudioDeviceName());
    m_settings->setCaptureCursor(ui->captureCursorCheckBox->isChecked());
    m_settings->setRecordingFolder(ui->recordingFolderLabel->text());
}

Qn::CaptureMode QnRecordingSettingsWidget::captureMode() const
{
    if (ui->fullscreenButton->isChecked()) {
        bool noAero = ui->disableAeroCheckBox->isChecked() &&
                ui->disableAeroCheckBox->isVisible() &&
                ui->disableAeroCheckBox->isEnabled();

        if (noAero)
            return Qn::FullScreenNoAeroMode;
        return Qn::FullScreenMode;
    }
    return Qn::WindowMode;
}

void QnRecordingSettingsWidget::setCaptureMode(Qn::CaptureMode c)
{
    switch (c) {
    case Qn::FullScreenMode:
        ui->fullscreenButton->setChecked(true);
        ui->disableAeroCheckBox->setChecked(false);
        break;
    case Qn::FullScreenNoAeroMode:
        ui->fullscreenButton->setChecked(true);
        ui->disableAeroCheckBox->setChecked(true);
        break;
    case Qn::WindowMode:
        ui->windowButton->setChecked(true);
        break;
    default:
        break;
    }
}

Qn::DecoderQuality QnRecordingSettingsWidget::decoderQuality() const
{
    return (Qn::DecoderQuality)ui->qualityComboBox->currentIndex();
}

void QnRecordingSettingsWidget::setDecoderQuality(Qn::DecoderQuality q)
{
    ui->qualityComboBox->setCurrentIndex(q);
}

Qn::Resolution QnRecordingSettingsWidget::resolution() const
{
    int index = ui->resolutionComboBox->currentIndex();
#ifdef CL_TRIAL_MODE
    index += (int) QnVideoRecorderSettings::Res640x480; // prev elements are skipped
#endif
    return (Qn::Resolution) index;
}

void QnRecordingSettingsWidget::setResolution(Qn::Resolution r)
{
#ifdef CL_TRIAL_MODE
    ui->resolutionComboBox->setCurrentIndex(r - (int) Qn::Exact640x480Resolution);
#else
    ui->resolutionComboBox->setCurrentIndex(r);
#endif
}

int QnRecordingSettingsWidget::screen() const
{
    return ui->screenComboBox->itemData(ui->screenComboBox->currentIndex()).toInt();
}

void QnRecordingSettingsWidget::setScreen(int screen)
{
    ui->screenComboBox->setCurrentIndex(screen);
}

QString QnRecordingSettingsWidget::primaryAudioDeviceName() const
{
    return ui->primaryAudioDeviceComboBox->currentText();
}

void QnRecordingSettingsWidget::setPrimaryAudioDeviceName(const QString &name)
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

QString QnRecordingSettingsWidget::secondaryAudioDeviceName() const
{
    return ui->secondaryAudioDeviceComboBox->currentText();
}

void QnRecordingSettingsWidget::setSecondaryAudioDeviceName(const QString &name)
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

void QnRecordingSettingsWidget::additionalAdjustSize()
{
    ui->primaryDeviceTextLabel->adjustSize();
    ui->secondaryDeviceTextLabel->adjustSize();

    int maxWidth = qMax(ui->primaryDeviceTextLabel->width(), ui->secondaryDeviceTextLabel->width());
    QSize s = ui->primaryDeviceTextLabel->minimumSize();
    s.setWidth(maxWidth);
    ui->primaryDeviceTextLabel->setMinimumSize(s);
    ui->secondaryDeviceTextLabel->setMinimumSize(s);

    ui->primaryDeviceTextLabel->adjustSize();
    ui->secondaryDeviceTextLabel->adjustSize();
}

void QnRecordingSettingsWidget::updateRecordingWarning() {
    ui->recordingWarningLabel->setVisible(decoderQuality() == Qn::BestQuality &&
                                          (resolution() == Qn::Exact1920x1080Resolution || resolution() == Qn::NativeResolution ));
}

void QnRecordingSettingsWidget::updateDisableAeroCheckbox() {
    bool isPrimary = ui->screenComboBox->itemData(ui->screenComboBox->currentIndex()) == qApp->desktop()->primaryScreen();
    ui->disableAeroCheckBox->setEnabled(isPrimary);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRecordingSettingsWidget::onComboboxChanged(int index)
{
    additionalAdjustSize();
#ifdef Q_OS_WIN
    QComboBox* c = (QComboBox*) sender();
    WinAudioExtendInfo info(c->itemText(index));
    QLabel* l = c == ui->primaryAudioDeviceComboBox ? ui->primaryDeviceIconLabel : ui->secondaryDeviceIconLabel;
    QPixmap icon = info.deviceIcon();
    if (!icon.isNull())
        l->setPixmap(icon.scaled(ICON_SIZE, ICON_SIZE));
    else
        setDefaultSoundIcon(l);
#else
    Q_UNUSED(index)
#endif
}

void QnRecordingSettingsWidget::at_browseRecordingFolderButton_clicked(){
    QFileDialog fileDialog(this);
    fileDialog.setDirectory(ui->recordingFolderLabel->text());
    fileDialog.setFileMode(QFileDialog::DirectoryOnly);
    if (!fileDialog.exec())
        return;

    QString dir = QDir::toNativeSeparators(fileDialog.selectedFiles().first());
    if (dir.isEmpty())
        return;

    ui->recordingFolderLabel->setText(dir);
}

void QnRecordingSettingsWidget::at_dwm_compositionChanged(bool enabled) {
    ui->disableAeroCheckBox->setVisible(m_dwm->isSupported() && enabled);
}
