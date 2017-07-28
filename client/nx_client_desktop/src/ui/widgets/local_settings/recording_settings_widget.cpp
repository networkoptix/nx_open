#include "recording_settings_widget.h"
#include "ui_recording_settings_widget.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/dwm.h>
#include <ui/workaround/widgets_signals_workaround.h>

QnRecordingSettingsWidget::QnRecordingSettingsWidget(QnVideoRecorderSettings* settings, QWidget *parent):
    base_type(parent),
    ui(new Ui::RecordingSettings),
    m_settings(settings),
    m_dwm(new QnDwm(this))
{
    ui->setupUi(this);

    initScreenComboBox();
    initQualityCombobox();
    initResolutionCombobox();

    setWarningStyle(ui->recordingWarningLabel);
    setHelpTopic(this, Qn::SystemSettings_ScreenRecording_Help);

    connect(ui->disableAeroCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->captureCursorCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->browseRecordingFolderButton, &QPushButton::clicked, this,
        &QnRecordingSettingsWidget::at_browseRecordingFolderButton_clicked);

    connect(m_dwm, &QnDwm::compositionChanged, this,
        &QnRecordingSettingsWidget::at_dwm_compositionChanged);

    at_dwm_compositionChanged();
    updateDisableAeroCheckbox();
    updateRecordingWarning();
}

QnRecordingSettingsWidget::~QnRecordingSettingsWidget()
{
}

void QnRecordingSettingsWidget::loadDataToUi()
{
    setCaptureMode(m_settings->captureMode());
    setDecoderQuality(m_settings->decoderQuality());
    setResolution(m_settings->resolution());
    setScreen(m_settings->screen());
    ui->captureCursorCheckBox->setChecked(m_settings->captureCursor());
    ui->recordingFolderLineEdit->setText(m_settings->recordingFolder());
}

void QnRecordingSettingsWidget::applyChanges()
{
    bool isChanged = false;

    if (m_settings->captureMode() != captureMode())
    {
        m_settings->setCaptureMode(captureMode());
        isChanged = true;
    }

    if (m_settings->decoderQuality() != decoderQuality())
    {
        m_settings->setDecoderQuality(decoderQuality());
        isChanged = true;
    }

    if (m_settings->resolution() != resolution())
    {
        m_settings->setResolution(resolution());
        isChanged = true;
    }

    if (m_settings->screen() != screen())
    {
        m_settings->setScreen(screen());
        isChanged = true;
    }

    if (m_settings->captureCursor() != ui->captureCursorCheckBox->isChecked())
    {
        m_settings->setCaptureCursor(ui->captureCursorCheckBox->isChecked());
        isChanged = true;
    }

    if (m_settings->recordingFolder() != ui->recordingFolderLineEdit->text())
    {
        m_settings->setRecordingFolder(ui->recordingFolderLineEdit->text());
        isChanged = true;
    }

    if (isChanged)
        emit recordingSettingsChanged();
}

bool QnRecordingSettingsWidget::hasChanges() const
{
    // TODO: #GDM refactor and emit hasChangesChanged correctly

    if (m_settings->captureMode() != captureMode())
        return true;

    if (m_settings->decoderQuality() != decoderQuality())
        return true;

    if (m_settings->resolution() != resolution())
        return true;

    if (m_settings->screen() != screen())
        return true;

    if (m_settings->captureCursor() != ui->captureCursorCheckBox->isChecked())
        return true;

    if (m_settings->recordingFolder() != ui->recordingFolderLineEdit->text())
        return true;

    return false;
}


void QnRecordingSettingsWidget::initScreenComboBox()
{
    QDesktopWidget *desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); i++)
    {
        bool isPrimaryScreen = (i == desktop->primaryScreen());
        if (!m_dwm->isSupported() && !isPrimaryScreen)
            continue; // TODO: #GDM can we record from secondary screen without DWM?

        QRect geometry = desktop->screenGeometry(i);
        QString item = tr("Screen %1 - %2x%3")
            .arg(i + 1)
            .arg(geometry.width())
            .arg(geometry.height());
        if (isPrimaryScreen)
            item = tr("%1 (Primary)").arg(item);
        ui->screenComboBox->addItem(item, i);
    }

    const bool screenSelectorVisible = ui->screenComboBox->count() > 0;
    ui->screenComboBox->setVisible(screenSelectorVisible);
    ui->screenLabel->setVisible(screenSelectorVisible);

    connect(ui->screenComboBox, QnComboboxCurrentIndexChanged, this,
        &QnRecordingSettingsWidget::updateDisableAeroCheckbox);
    connect(ui->screenComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

void QnRecordingSettingsWidget::initQualityCombobox()
{
    ui->qualityComboBox->addItem(tr("Best"), Qn::BestQuality);
    ui->qualityComboBox->addItem(tr("Average"), Qn::BalancedQuality);
    ui->qualityComboBox->addItem(tr("Performance"), Qn::PerformanceQuality);

    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this,
        &QnRecordingSettingsWidget::updateRecordingWarning);
    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

void QnRecordingSettingsWidget::initResolutionCombobox()
{
    ui->resolutionComboBox->addItem(tr("Native"), Qn::NativeResolution);
    ui->resolutionComboBox->addItem(tr("Quarter Native"), Qn::QuaterNativeResolution);
    ui->resolutionComboBox->addItem(tr("1920x1080"), Qn::Exact1920x1080Resolution);
    ui->resolutionComboBox->addItem(tr("1280x720"), Qn::Exact1280x720Resolution);
    ui->resolutionComboBox->addItem(tr("640x480"), Qn::Exact640x480Resolution);
    ui->resolutionComboBox->addItem(tr("320x240"), Qn::Exact320x240Resolution);
    ui->resolutionComboBox->setCurrentIndex(0);

    connect(ui->resolutionComboBox, QnComboboxCurrentIndexChanged, this,
        &QnRecordingSettingsWidget::updateRecordingWarning);
    connect(ui->resolutionComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

bool QnRecordingSettingsWidget::isPrimaryScreenSelected() const
{
    const auto desktop = qApp->desktop();
    return ((desktop->screenCount() == 1) || (screen() == desktop->primaryScreen()));
}

Qn::CaptureMode QnRecordingSettingsWidget::captureMode() const
{
    if (!m_dwm->isSupported() || !m_dwm->isCompositionEnabled())
        return Qn::FullScreenMode; // no need to disable aero if dwm is disabled

    if (!isPrimaryScreenSelected())
        return Qn::FullScreenMode; // recording from secondary screen without aero is not supported

    return (ui->disableAeroCheckBox->isChecked()
        ? Qn::FullScreenNoAeroMode
        : Qn::FullScreenMode);
}

void QnRecordingSettingsWidget::setCaptureMode(Qn::CaptureMode c)
{
    ui->disableAeroCheckBox->setChecked(c == Qn::FullScreenNoAeroMode);
}

Qn::DecoderQuality QnRecordingSettingsWidget::decoderQuality() const
{
    return static_cast<Qn::DecoderQuality>(ui->qualityComboBox->currentData().toInt());
}

void QnRecordingSettingsWidget::setDecoderQuality(Qn::DecoderQuality q)
{
    int index = ui->qualityComboBox->findData(q);
    ui->qualityComboBox->setCurrentIndex(index);
}

Qn::Resolution QnRecordingSettingsWidget::resolution() const
{
    return static_cast<Qn::Resolution>(ui->resolutionComboBox->currentData().toInt());
}

void QnRecordingSettingsWidget::setResolution(Qn::Resolution r)
{
    int index = ui->resolutionComboBox->findData(r);
    ui->resolutionComboBox->setCurrentIndex(index);
}

int QnRecordingSettingsWidget::screen() const
{
    return ui->screenComboBox->itemData(ui->screenComboBox->currentIndex()).toInt();
}

void QnRecordingSettingsWidget::setScreen(int screen)
{
    ui->screenComboBox->setCurrentIndex(screen);
}

void QnRecordingSettingsWidget::updateRecordingWarning()
{
    ui->recordingWarningLabel->setVisible(decoderQuality() == Qn::BestQuality
        && (resolution() == Qn::Exact1920x1080Resolution
            || resolution() == Qn::NativeResolution));
}

void QnRecordingSettingsWidget::updateDisableAeroCheckbox()
{
    // without Aero only recording from primary screen is supported

    const bool isPrimary = isPrimaryScreenSelected();
    ui->disableAeroCheckBox->setEnabled(isPrimary);
    if (!isPrimary)
        ui->disableAeroCheckBox->setChecked(false);

    emit hasChangesChanged();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRecordingSettingsWidget::at_browseRecordingFolderButton_clicked()
{
    QString dirName = QnFileDialog::getExistingDirectory(this,
        tr("Select folder..."),
        ui->recordingFolderLineEdit->text(),
        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;
    ui->recordingFolderLineEdit->setText(dirName);
    emit hasChangesChanged();
}

void QnRecordingSettingsWidget::at_dwm_compositionChanged()
{
    /* Aero is already disabled if dwm is not enabled or not supported. */
    ui->disableAeroCheckBox->setVisible(m_dwm->isSupported() && m_dwm->isCompositionEnabled());
}
