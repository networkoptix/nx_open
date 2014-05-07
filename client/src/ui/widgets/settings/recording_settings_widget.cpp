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
#include <ui/workbench/workbench_context.h>

#ifdef Q_OS_WIN
#   include <device_plugins/desktop_win/win_audio_device_info.h>
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
    m_settings(new QnVideoRecorderSettings(this))
{
    ui->setupUi(this);

    foreach (const QString& deviceName, QnVideoRecorderSettings::availableDeviceNames(QAudio::AudioInput)) {
        ui->primaryAudioDeviceComboBox->addItem(deviceName);
        ui->secondaryAudioDeviceComboBox->addItem(deviceName);
    }

    setHelpTopic(this, Qn::SystemSettings_ScreenRecording_Help);

    connect(ui->qualityComboBox,                SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateRecordingWarning()));
    connect(ui->resolutionComboBox,             SIGNAL(currentIndexChanged(int)),   this,   SLOT(updateRecordingWarning()));
    connect(ui->primaryAudioDeviceComboBox,     SIGNAL(currentIndexChanged(int)),   this,   SLOT(onComboboxChanged(int)));
    connect(ui->secondaryAudioDeviceComboBox,   SIGNAL(currentIndexChanged(int)),   this,   SLOT(onComboboxChanged(int)));
    connect(ui->browseRecordingFolderButton,    SIGNAL(clicked()),                  this,   SLOT(at_browseRecordingFolderButton_clicked()));

#ifdef Q_OS_WIN
    connect(this, SIGNAL(recordingSettingsChanged()), this->context()->instance<QnWorkbenchDesktopCameraWatcher>(), SLOT(at_recordingSettingsChanged()));
#endif

    setWarningStyle(ui->recordingWarningLabel);
    setDefaultSoundIcon(ui->primaryDeviceIconLabel);
    setDefaultSoundIcon(ui->secondaryDeviceIconLabel);

    updateRecordingWarning();
}

QnRecordingSettingsWidget::~QnRecordingSettingsWidget() {
}

void QnRecordingSettingsWidget::updateFromSettings() {
    setDecoderQuality(m_settings->decoderQuality());
    setResolution(m_settings->resolution());
    setPrimaryAudioDeviceName(m_settings->primaryAudioDevice().fullName());
    setSecondaryAudioDeviceName(m_settings->secondaryAudioDevice().fullName());
    
    ui->captureCursorCheckBox->setChecked(m_settings->captureCursor());
    ui->recordingFolderLabel->setText(m_settings->recordingFolder());
}

void QnRecordingSettingsWidget::submitToSettings() 
{
    bool isChanged = false;
    
    if (m_settings->decoderQuality() != decoderQuality()) {
        m_settings->setDecoderQuality(decoderQuality());
        isChanged = true;
    }

    if (m_settings->resolution() != resolution()) {
        m_settings->setResolution(resolution());
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

Qn::DecoderQuality QnRecordingSettingsWidget::decoderQuality() const
{
    return (Qn::DecoderQuality)ui->qualityComboBox->currentIndex();
}

void QnRecordingSettingsWidget::setDecoderQuality(Qn::DecoderQuality q)
{
    ui->qualityComboBox->setCurrentIndex(q);
}

Qn::Resolution QnRecordingSettingsWidget::resolution() const {
    int index = ui->resolutionComboBox->currentIndex();
    return (Qn::Resolution) index;
}

void QnRecordingSettingsWidget::setResolution(Qn::Resolution r) {
    ui->resolutionComboBox->setCurrentIndex(r);
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
