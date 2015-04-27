#include "recording_settings_widget.h"
#include "ui_recording_settings_widget.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtMultimedia/QAudioDeviceInfo>

#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>
#include <ui/widgets/dwm.h>
#include <ui/workbench/workbench_context.h>

#ifdef Q_OS_WIN
#   include <plugins/resource/desktop_win/win_audio_device_info.h>
#   include <ui/workbench/watchers/workbench_desktop_camera_watcher_win.h>
#endif


namespace {
    const int ICON_SIZE = 32;

    void setDefaultSoundIcon(QLabel *label) {
        label->setPixmap(qnSkin->pixmap("microphone.png", QSize(ICON_SIZE, ICON_SIZE), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

} // anonymous namespace


QnRecordingSettingsWidget::QnRecordingSettingsWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::RecordingSettings),
    m_settings(new QnVideoRecorderSettings(this)),
    m_dwm(new QnDwm(this))
{
    ui->setupUi(this);

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

    setHelpTopic(this, Qn::SystemSettings_ScreenRecording_Help);

    connect(ui->fullscreenButton,               SIGNAL(toggled(bool)),              ui->screenComboBox,         SLOT(setEnabled(bool)));
    connect(ui->fullscreenButton,               SIGNAL(toggled(bool)),              ui->disableAeroCheckBox,    SLOT(setEnabled(bool)));

    connect(ui->qualityComboBox,                SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateRecordingWarning()));
    connect(ui->resolutionComboBox,             SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateRecordingWarning()));
    connect(ui->primaryAudioDeviceComboBox,     SIGNAL(currentIndexChanged(int)),   this,   SLOT(onComboboxChanged(int)));
    connect(ui->secondaryAudioDeviceComboBox,   SIGNAL(currentIndexChanged(int)),   this,   SLOT(onComboboxChanged(int)));
    connect(ui->screenComboBox,                 SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateDisableAeroCheckbox()));
    connect(ui->browseRecordingFolderButton,    SIGNAL(clicked()),                  this,   SLOT(at_browseRecordingFolderButton_clicked()));
    connect(m_dwm,                              SIGNAL(compositionChanged()),       this,   SLOT(at_dwm_compositionChanged()));

#ifdef Q_OS_WIN
    connect(this, &QnRecordingSettingsWidget::recordingSettingsChanged, this->context()->instance<QnWorkbenchDesktopCameraWatcher>(), &QnWorkbenchDesktopCameraWatcher::forcedUpdate);
#endif

    setWarningStyle(ui->recordingWarningLabel);
    setDefaultSoundIcon(ui->primaryDeviceIconLabel);
    setDefaultSoundIcon(ui->secondaryDeviceIconLabel);

    at_dwm_compositionChanged();
    updateDisableAeroCheckbox();
    updateRecordingWarning();
}

QnRecordingSettingsWidget::~QnRecordingSettingsWidget() {
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

void QnRecordingSettingsWidget::submitToSettings() 
{
    bool isChanged = false;
    
    if (m_settings->captureMode() != captureMode()) {
        m_settings->setCaptureMode(captureMode());
        isChanged = true;
    }

    if (m_settings->decoderQuality() != decoderQuality()) {
        m_settings->setDecoderQuality(decoderQuality());
        isChanged = true;
    }

    if (m_settings->resolution() != resolution()) {
        m_settings->setResolution(resolution());
        isChanged = true;
    }

    if (m_settings->screen() != screen()) {
        m_settings->setScreen(screen());
        isChanged = true;
    }

    if (m_settings->primaryAudioDeviceName() != primaryAudioDeviceName()) {
        m_settings->setPrimaryAudioDeviceByName(primaryAudioDeviceName());
        isChanged = true;
    }

    if (m_settings->secondaryAudioDeviceName() != secondaryAudioDeviceName()) {
        m_settings->setSecondaryAudioDeviceByName(secondaryAudioDeviceName());
        isChanged = true;
    }

    if (m_settings->captureCursor() != ui->captureCursorCheckBox->isChecked()) {
        m_settings->setCaptureCursor(ui->captureCursorCheckBox->isChecked());
        isChanged = true;
    }

    if (m_settings->recordingFolder() != ui->recordingFolderLabel->text()) {
        m_settings->setRecordingFolder(ui->recordingFolderLabel->text());
        isChanged = true;
    }

    if (isChanged)
        emit recordingSettingsChanged();
}

Qn::CaptureMode QnRecordingSettingsWidget::captureMode() const
{
    if (ui->fullscreenButton->isChecked()) {
        if (!m_dwm->isSupported() || !m_dwm->isCompositionEnabled())
            return Qn::FullScreenMode; // no need to disable aero if dwm is disabled

        bool isPrimary = ui->screenComboBox->itemData(ui->screenComboBox->currentIndex()) == qApp->desktop()->primaryScreen();
        if (!isPrimary)
            return Qn::FullScreenMode; // recording from secondary screen without aero is not supported

        return (ui->disableAeroCheckBox->isChecked())
                ? Qn::FullScreenNoAeroMode
                : Qn::FullScreenMode;
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
    // TODO: #Elric. Very bad. Text is set in Designer, enum values in C++ code. 
    // Text should be filled in code, and no assumptions should be made about enum values.
    return (Qn::DecoderQuality)ui->qualityComboBox->currentIndex(); 
}

void QnRecordingSettingsWidget::setDecoderQuality(Qn::DecoderQuality q)
{
    ui->qualityComboBox->setCurrentIndex(q);
}

Qn::Resolution QnRecordingSettingsWidget::resolution() const {
    // TODO: #Elric same thing here. ^
    int index = ui->resolutionComboBox->currentIndex();
    return (Qn::Resolution) index;
}

void QnRecordingSettingsWidget::setResolution(Qn::Resolution r) {
    ui->resolutionComboBox->setCurrentIndex(r);
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
    // without Aero only recording from primary screen is supported
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
    QnWinAudioDeviceInfo info(c->itemText(index));
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
    QString dirName = QnFileDialog::getExistingDirectory(this,
                                                        tr("Select folder..."),
                                                        ui->recordingFolderLabel->text(),
                                                        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;
    ui->recordingFolderLabel->setText(dirName);
}

void QnRecordingSettingsWidget::at_dwm_compositionChanged() {
    /* Aero is already disabled if dwm is not enabled or not supported. */
    ui->disableAeroCheckBox->setVisible(m_dwm->isSupported() && m_dwm->isCompositionEnabled());
}
