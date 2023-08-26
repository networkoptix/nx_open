// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_settings_widget.h"
#include "ui_recording_settings_widget.h"

#include <QtGui/QScreen>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/screen_recording_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/widgets/dwm.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

using namespace screen_recording;

RecordingSettingsWidget::RecordingSettingsWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::RecordingSettingsWidget),
    m_dwm(new QnDwm(this))
{
    ui->setupUi(this);

    initScreenComboBox();
    initQualityCombobox();
    initResolutionCombobox();

    setWarningStyle(ui->recordingWarningLabel);
    setHelpTopic(this, HelpTopic::Id::SystemSettings_ScreenRecording);

    connect(ui->captureCursorCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->browseRecordingFolderButton, &QPushButton::clicked, this,
        &RecordingSettingsWidget::at_browseRecordingFolderButton_clicked);

    updateRecordingWarning();
}

RecordingSettingsWidget::~RecordingSettingsWidget()
{
}

void RecordingSettingsWidget::loadDataToUi()
{
    setQuality(screenRecordingSettings()->quality());
    setResolution(screenRecordingSettings()->resolution());
    setScreen(screenRecordingSettings()->screen());
    ui->captureCursorCheckBox->setChecked(screenRecordingSettings()->captureCursor());
    ui->recordingFolderLineEdit->setText(screenRecordingSettings()->recordingFolder());
}

void RecordingSettingsWidget::applyChanges()
{
    bool isChanged = false;

    if (screenRecordingSettings()->captureMode() != captureMode())
    {
        screenRecordingSettings()->captureMode = captureMode();
        isChanged = true;
    }

    if (screenRecordingSettings()->quality() != quality())
    {
        screenRecordingSettings()->quality = quality();
        isChanged = true;
    }

    if (screenRecordingSettings()->resolution() != resolution())
    {
        screenRecordingSettings()->resolution = resolution();
        isChanged = true;
    }

    if (screenRecordingSettings()->screen() != screen())
    {
        screenRecordingSettings()->setScreen(screen());
        isChanged = true;
    }

    if (screenRecordingSettings()->captureCursor() != ui->captureCursorCheckBox->isChecked())
    {
        screenRecordingSettings()->captureCursor = ui->captureCursorCheckBox->isChecked();
        isChanged = true;
    }

    if (screenRecordingSettings()->recordingFolder() != ui->recordingFolderLineEdit->text())
    {
        screenRecordingSettings()->recordingFolder = ui->recordingFolderLineEdit->text();
        isChanged = true;
    }

    if (isChanged)
        emit recordingSettingsChanged();
}

bool RecordingSettingsWidget::hasChanges() const
{
    // TODO: #sivanov Refactor and emit hasChangesChanged correctly.

    if (screenRecordingSettings()->captureMode() != captureMode())
        return true;

    if (screenRecordingSettings()->quality() != quality())
        return true;

    if (screenRecordingSettings()->resolution() != resolution())
        return true;

    if (screenRecordingSettings()->screen() != screen())
        return true;

    if (screenRecordingSettings()->captureCursor() != ui->captureCursorCheckBox->isChecked())
        return true;

    if (screenRecordingSettings()->recordingFolder() != ui->recordingFolderLineEdit->text())
        return true;

    return false;
}

void RecordingSettingsWidget::initScreenComboBox()
{
    const auto screens = QGuiApplication::screens();
    for (qsizetype i = 0; i < screens.size(); ++i)
    {
        bool isPrimaryScreen = screens.at(i) == qApp->primaryScreen();
        if (!m_dwm->isSupported() && !isPrimaryScreen)
            continue; //< TODO: #sivanov Can we record from secondary screen without DWM?

        QRect geometry = screens.at(i)->geometry();
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
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

void RecordingSettingsWidget::initQualityCombobox()
{
    ui->qualityComboBox->addItem(tr("Best"), QVariant::fromValue(Quality::best));
    ui->qualityComboBox->addItem(tr("Average"), QVariant::fromValue(Quality::balanced));
    ui->qualityComboBox->addItem(tr("Performance"), QVariant::fromValue(Quality::performance));

    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this,
        &RecordingSettingsWidget::updateRecordingWarning);
    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

void RecordingSettingsWidget::initResolutionCombobox()
{
    ui->resolutionComboBox->addItem(tr("Native"), QVariant::fromValue(Resolution::native));
    ui->resolutionComboBox->addItem(tr("Quarter Native"),
        QVariant::fromValue(Resolution::quarterNative));
    ui->resolutionComboBox->addItem("1920x1080", QVariant::fromValue(Resolution::_1920x1080));
    ui->resolutionComboBox->addItem("1280x720", QVariant::fromValue(Resolution::_1280x720));
    ui->resolutionComboBox->addItem("640x480", QVariant::fromValue(Resolution::_640x480));
    ui->resolutionComboBox->addItem("320x240", QVariant::fromValue(Resolution::_320x240));
    ui->resolutionComboBox->setCurrentIndex(0);

    connect(ui->resolutionComboBox, QnComboboxCurrentIndexChanged, this,
        &RecordingSettingsWidget::updateRecordingWarning);
    connect(ui->resolutionComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

bool RecordingSettingsWidget::isPrimaryScreenSelected() const
{
    const auto screens = QGuiApplication::screens();
    return (screens.size() == 1) || (screen() == screens.indexOf(qApp->primaryScreen()));
}

CaptureMode RecordingSettingsWidget::captureMode() const
{
    return CaptureMode::fullScreen;
}

Quality RecordingSettingsWidget::quality() const
{
    return ui->qualityComboBox->currentData().value<Quality>();
}

void RecordingSettingsWidget::setQuality(Quality quality)
{
    int index = ui->qualityComboBox->findData(QVariant::fromValue(quality));
    ui->qualityComboBox->setCurrentIndex(index);
}

Resolution RecordingSettingsWidget::resolution() const
{
    return ui->resolutionComboBox->currentData().value<Resolution>();
}

void RecordingSettingsWidget::setResolution(Resolution resolution)
{
    int index = ui->resolutionComboBox->findData(QVariant::fromValue(resolution));
    ui->resolutionComboBox->setCurrentIndex(index);
}

int RecordingSettingsWidget::screen() const
{
    return ui->screenComboBox->itemData(ui->screenComboBox->currentIndex()).toInt();
}

void RecordingSettingsWidget::setScreen(int screen)
{
    ui->screenComboBox->setCurrentIndex(screen);
}

void RecordingSettingsWidget::updateRecordingWarning()
{
    ui->recordingWarningLabel->setVisible(quality() == Quality::best
        && (resolution() == Resolution::_1920x1080 || resolution() == Resolution::native));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void RecordingSettingsWidget::at_browseRecordingFolderButton_clicked()
{
    QString dirName = QFileDialog::getExistingDirectory(this,
        tr("Select Folder..."),
        ui->recordingFolderLineEdit->text(),
        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;
    ui->recordingFolderLineEdit->setText(dirName);
    emit hasChangesChanged();
}

} // namespace nx::vms::client::desktop
