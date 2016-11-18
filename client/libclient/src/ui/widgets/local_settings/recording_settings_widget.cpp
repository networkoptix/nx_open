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
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/dwm.h>

#if defined(Q_OS_WIN)
#   include <plugins/resource/desktop_win/win_audio_device_info.h>
#endif


QnRecordingSettingsWidget::QnRecordingSettingsWidget(QnVideoRecorderSettings* settings, QWidget *parent):
    base_type(parent),
    ui(new Ui::RecordingSettings),
    m_settings(settings),
    m_dwm(new QnDwm(this))
{
    ui->setupUi(this);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    QDesktopWidget *desktop = qApp->desktop();
    for (int i = 0; i < desktop->screenCount(); i++)
    {
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

    setWarningStyle(ui->recordingWarningLabel);
    setHelpTopic(this, Qn::SystemSettings_ScreenRecording_Help);

    connect(ui->fullscreenButton, SIGNAL(toggled(bool)), ui->screenComboBox, SLOT(setEnabled(bool)));
    connect(ui->fullscreenButton, SIGNAL(toggled(bool)), ui->disableAeroCheckBox, SLOT(setEnabled(bool)));

    connect(ui->qualityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateRecordingWarning()));
    connect(ui->resolutionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateRecordingWarning()));
    connect(ui->screenComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateDisableAeroCheckbox()));
    connect(ui->browseRecordingFolderButton, SIGNAL(clicked()), this, SLOT(at_browseRecordingFolderButton_clicked()));
    connect(m_dwm, SIGNAL(compositionChanged()), this, SLOT(at_dwm_compositionChanged()));

    at_dwm_compositionChanged();
    updateDisableAeroCheckbox();
    updateRecordingWarning();
}

QnRecordingSettingsWidget::~QnRecordingSettingsWidget()
{}

void QnRecordingSettingsWidget::loadDataToUi()
{
    setCaptureMode(m_settings->captureMode());
    setDecoderQuality(m_settings->decoderQuality());
    setResolution(m_settings->resolution());
    setScreen(m_settings->screen());
    ui->captureCursorCheckBox->setChecked(m_settings->captureCursor());
    ui->recordingFolderLabel->setText(m_settings->recordingFolder());
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

    if (m_settings->recordingFolder() != ui->recordingFolderLabel->text())
    {
        m_settings->setRecordingFolder(ui->recordingFolderLabel->text());
        isChanged = true;
    }

    if (isChanged)
        emit recordingSettingsChanged();
}

bool QnRecordingSettingsWidget::hasChanges() const
{
    //TODO: #GDM refactor and emit hasChangesChanged correctly

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

    if (m_settings->recordingFolder() != ui->recordingFolderLabel->text())
        return true;

    return false;
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

void QnRecordingSettingsWidget::additionalAdjustSize()
{
//     ui->primaryDeviceTextLabel->adjustSize();
//     ui->secondaryDeviceTextLabel->adjustSize();
//
//     int maxWidth = qMax(ui->primaryDeviceTextLabel->width(), ui->secondaryDeviceTextLabel->width());
//     QSize s = ui->primaryDeviceTextLabel->minimumSize();
//     s.setWidth(maxWidth);
//     ui->primaryDeviceTextLabel->setMinimumSize(s);
//     ui->secondaryDeviceTextLabel->setMinimumSize(s);
//
//     ui->primaryDeviceTextLabel->adjustSize();
//     ui->secondaryDeviceTextLabel->adjustSize();
}

void QnRecordingSettingsWidget::updateRecordingWarning()
{
    ui->recordingWarningLabel->setVisible(decoderQuality() == Qn::BestQuality
        && (resolution() == Qn::Exact1920x1080Resolution
            || resolution() == Qn::NativeResolution ));
}

void QnRecordingSettingsWidget::updateDisableAeroCheckbox()
{
    // without Aero only recording from primary screen is supported
    bool isPrimary = ui->screenComboBox->itemData(ui->screenComboBox->currentIndex()) == qApp->desktop()->primaryScreen();
    ui->disableAeroCheckBox->setEnabled(isPrimary);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnRecordingSettingsWidget::at_browseRecordingFolderButton_clicked()
{
    QString dirName = QnFileDialog::getExistingDirectory(this,
                                                        tr("Select folder..."),
                                                        ui->recordingFolderLabel->text(),
                                                        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;
    ui->recordingFolderLabel->setText(dirName);
}

void QnRecordingSettingsWidget::at_dwm_compositionChanged()
{
    /* Aero is already disabled if dwm is not enabled or not supported. */
    ui->disableAeroCheckBox->setVisible(m_dwm->isSupported() && m_dwm->isCompositionEnabled());
}
