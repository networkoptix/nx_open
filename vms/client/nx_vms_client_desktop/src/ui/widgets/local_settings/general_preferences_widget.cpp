// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "general_preferences_widget.h"
#include "ui_general_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtMultimedia/QAudioDevice>

#include <common/common_module.h>
#include <nx/build_info.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/screen_recording_settings.h>
#include <nx/vms/utils/platform/autorun.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workaround/widgets_signals_workaround.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {
const int kMsecsPerMinute = 60 * 1000;
}

QnGeneralPreferencesWidget::QnGeneralPreferencesWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::GeneralPreferencesWidget)
{
    ui->setupUi(this);

    if (!nx::vms::utils::isAutoRunSupported())
        ui->autoStartCheckBox->hide();

    setHelpTopic(ui->mediaFoldersGroupBox, HelpTopic::Id::MediaFolders);
    setHelpTopic(ui->pauseOnInactivityCheckBox,
        HelpTopic::Id::SystemSettings_General_AutoPause);
    setHelpTopic(ui->idleTimeoutSpinBox, ui->idleTimeoutWidget,
        HelpTopic::Id::SystemSettings_General_AutoPause);

    ui->idleTimeoutWidget->setEnabled(false);

    initAudioDevices();

    connect(ui->addMediaFolderButton, &QPushButton::clicked, this,
        &QnGeneralPreferencesWidget::at_addMediaFolderButton_clicked);
    connect(ui->removeMediaFolderButton, &QPushButton::clicked, this,
        &QnGeneralPreferencesWidget::at_removeMediaFolderButton_clicked);
    connect(ui->mediaFoldersList->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &QnGeneralPreferencesWidget::at_mediaFoldersList_selectionChanged);

    connect(ui->pauseOnInactivityCheckBox, &QCheckBox::toggled, ui->idleTimeoutWidget,
        &QWidget::setEnabled);
    connect(ui->pauseOnInactivityCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->idleTimeoutSpinBox, QnSpinboxIntValueChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->autoStartCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->autoLoginCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->restoreSessionCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->allowEnteringSleepModeCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->primaryAudioDeviceComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->secondaryAudioDeviceComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->playAudioForAllCamerasCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->muteOnAudioTransmitCheckBox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);

    if (nx::build_info::isLinux())
        ui->allowEnteringSleepModeCheckBox->setHidden(true); //< Not implemented for Linux.
}

QnGeneralPreferencesWidget::~QnGeneralPreferencesWidget()
{
}

void QnGeneralPreferencesWidget::applyChanges()
{
    appContext()->localSettings()->mediaFolders = mediaFolders();
    appContext()->localSettings()->userIdleTimeoutMs = userIdleTimeoutMs();
    appContext()->localSettings()->autoStart = autoStart();
    appContext()->localSettings()->playAudioForAllItems =
        ui->playAudioForAllCamerasCheckbox->isChecked();

    bool recordingSettingsChanged = false;
    if (screenRecordingSettings()->primaryAudioDeviceName() != primaryAudioDeviceName())
    {
        screenRecordingSettings()->primaryAudioDeviceName = primaryAudioDeviceName();
        recordingSettingsChanged = true;
    }

    if (screenRecordingSettings()->secondaryAudioDeviceName() != secondaryAudioDeviceName())
    {
        screenRecordingSettings()->secondaryAudioDeviceName = secondaryAudioDeviceName();
        recordingSettingsChanged = true;
    }

    appContext()->localSettings()->autoLogin = autoLogin();
    appContext()->localSettings()->restoreUserSessionData = restoreUserSessionData();

    appContext()->localSettings()->allowComputerEnteringSleepMode =
        allowComputerEnteringSleepMode();

    appContext()->localSettings()->muteOnAudioTransmit = muteOnAudioTransmit();

    if (recordingSettingsChanged)
        emit this->recordingSettingsChanged();
}

void QnGeneralPreferencesWidget::loadDataToUi()
{
    setMediaFolders(appContext()->localSettings()->mediaFolders());
    setUserIdleTimeoutMs(appContext()->localSettings()->userIdleTimeoutMs());
    setPrimaryAudioDeviceName(screenRecordingSettings()->primaryAudioDeviceName());
    setSecondaryAudioDeviceName(screenRecordingSettings()->secondaryAudioDeviceName());
    setAutoStart(appContext()->localSettings()->autoStart());
    setAutoLogin(appContext()->localSettings()->autoLogin());
    setRestoreUserSessionData(appContext()->localSettings()->restoreUserSessionData());
    setAllowComputerEnteringSleepMode(
        appContext()->localSettings()->allowComputerEnteringSleepMode());
    ui->playAudioForAllCamerasCheckbox->setChecked(
        appContext()->localSettings()->playAudioForAllItems());
    setMuteOnAudioTransmit(appContext()->localSettings()->muteOnAudioTransmit());
}

bool QnGeneralPreferencesWidget::hasChanges() const
{
    if (ui->playAudioForAllCamerasCheckbox->isChecked() !=
        appContext()->localSettings()->playAudioForAllItems())
    {
        return true;
    }

    if (appContext()->localSettings()->mediaFolders() != mediaFolders())
        return true;

    bool oldPauseOnInactivity = (appContext()->localSettings()->userIdleTimeoutMs() > 0);
    bool newPauseOnInactivity = userIdleTimeoutMs() > 0;
    if (oldPauseOnInactivity != newPauseOnInactivity)
        return true;

    /* Do not compare negative values. */
    if (newPauseOnInactivity)
    {
        if (appContext()->localSettings()->userIdleTimeoutMs() != userIdleTimeoutMs())
            return true;
    }

    if (nx::vms::utils::isAutoRunSupported())
    {
        if (appContext()->localSettings()->autoStart() != autoStart())
            return true;
    }

    if (screenRecordingSettings()->primaryAudioDeviceName() != primaryAudioDeviceName())
        return true;

    if (screenRecordingSettings()->secondaryAudioDeviceName() != secondaryAudioDeviceName())
        return true;

    if (appContext()->localSettings()->autoLogin() != autoLogin())
        return true;

    if (appContext()->localSettings()->restoreUserSessionData() != restoreUserSessionData())
        return true;

    if (appContext()->localSettings()->allowComputerEnteringSleepMode()
        != allowComputerEnteringSleepMode())
    {
        return true;
    }

    if (appContext()->localSettings()->muteOnAudioTransmit() != muteOnAudioTransmit())
        return true;

    return false;
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnGeneralPreferencesWidget::at_addMediaFolderButton_clicked()
{
    QString initialDir = ui->mediaFoldersList->count() == 0
        ? QString()
        : ui->mediaFoldersList->item(0)->text();

    QString dirName = QFileDialog::getExistingDirectory(this,
        tr("Select Folder..."),
        initialDir,
        QnCustomFileDialog::directoryDialogOptions());
    if (dirName.isEmpty())
        return;

    dirName = QDir::toNativeSeparators(dirName);

    if (mediaFolders().contains(dirName))
    {
        QnMessageBox::information(this, tr("Folder already added"));
        return;
    }

    ui->mediaFoldersList->addItem(dirName);

    emit hasChangesChanged();
}

void QnGeneralPreferencesWidget::at_removeMediaFolderButton_clicked()
{
    for (auto item : ui->mediaFoldersList->selectedItems())
        delete item;

    emit hasChangesChanged();
}

void QnGeneralPreferencesWidget::at_mediaFoldersList_selectionChanged()
{
    ui->removeMediaFolderButton->setEnabled(
        !ui->mediaFoldersList->selectedItems().isEmpty());
}

QStringList QnGeneralPreferencesWidget::mediaFolders() const
{
    QStringList result;
    for (int i = 0; i < ui->mediaFoldersList->count(); ++i)
        result << ui->mediaFoldersList->item(i)->text();
    return result;
}

void QnGeneralPreferencesWidget::setMediaFolders(const QStringList& value)
{
    ui->mediaFoldersList->clear();
    for (const auto &item : value)
        ui->mediaFoldersList->addItem(QDir::toNativeSeparators(item));
}

quint64 QnGeneralPreferencesWidget::userIdleTimeoutMs() const
{
    return ui->pauseOnInactivityCheckBox->isChecked()
        ? ui->idleTimeoutSpinBox->value() * kMsecsPerMinute
        : 0;
}

void QnGeneralPreferencesWidget::setUserIdleTimeoutMs(quint64 value)
{
    ui->pauseOnInactivityCheckBox->setChecked(value > 0);
    if (value > 0)
        ui->idleTimeoutSpinBox->setValue(value / kMsecsPerMinute); // convert to minutes
}

bool QnGeneralPreferencesWidget::autoStart() const
{
    return ui->autoStartCheckBox->isChecked();
}

void QnGeneralPreferencesWidget::setAutoStart(bool value)
{
    ui->autoStartCheckBox->setChecked(value);
}

void QnGeneralPreferencesWidget::initAudioDevices()
{
    ui->primaryAudioDeviceComboBox->clear();
    ui->secondaryAudioDeviceComboBox->clear();

    QString defaultPrimaryDeviceName =
        screenRecordingSettings()->defaultPrimaryAudioDevice().fullName();
    if (defaultPrimaryDeviceName.isEmpty())
        defaultPrimaryDeviceName = tr("None");
    ui->primaryAudioDeviceComboBox->addItem(tr("Auto (%1)").arg(defaultPrimaryDeviceName));
    ui->primaryAudioDeviceComboBox->addItem(tr("None"), core::AudioRecordingSettings::kNoDevice);

    QString defaultSecondaryDeviceName =
        screenRecordingSettings()->defaultSecondaryAudioDevice().fullName();
    if (defaultSecondaryDeviceName.isEmpty())
        defaultSecondaryDeviceName = tr("None");
    ui->secondaryAudioDeviceComboBox->addItem(tr("Auto (%1)").arg(defaultSecondaryDeviceName));
    ui->secondaryAudioDeviceComboBox->addItem(tr("None"), core::AudioRecordingSettings::kNoDevice);

    for (const auto& deviceName: screenRecordingSettings()->availableDevices())
    {
        const QString& name = deviceName.fullName();
        ui->primaryAudioDeviceComboBox->addItem(name, name);
        ui->secondaryAudioDeviceComboBox->addItem(name, name);
    }
}

QString QnGeneralPreferencesWidget::primaryAudioDeviceName() const
{
    return ui->primaryAudioDeviceComboBox->currentData().toString();
}

void QnGeneralPreferencesWidget::setPrimaryAudioDeviceName(const QString& name)
{
    if (!name.isEmpty())
    {
        for (int i = 0; i < ui->primaryAudioDeviceComboBox->count(); ++i)
        {
            if (ui->primaryAudioDeviceComboBox->itemData(i).toString() == name)
            {
                ui->primaryAudioDeviceComboBox->setCurrentIndex(i);
                return;
            }
        }
    }

    ui->primaryAudioDeviceComboBox->setCurrentIndex(0);
}

QString QnGeneralPreferencesWidget::secondaryAudioDeviceName() const
{
    return ui->secondaryAudioDeviceComboBox->currentData().toString();
}

void QnGeneralPreferencesWidget::setSecondaryAudioDeviceName(const QString &name)
{
    if (!name.isEmpty())
    {
        for (int i = 0; i < ui->secondaryAudioDeviceComboBox->count(); i++)
        {
            if (ui->secondaryAudioDeviceComboBox->itemData(i).toString() == name)
            {
                ui->secondaryAudioDeviceComboBox->setCurrentIndex(i);
                return;
            }
        }
    }

    ui->secondaryAudioDeviceComboBox->setCurrentIndex(0);
}

bool QnGeneralPreferencesWidget::autoLogin() const
{
    return ui->autoLoginCheckBox->isChecked();
}

void QnGeneralPreferencesWidget::setAutoLogin(bool value)
{
    ui->autoLoginCheckBox->setChecked(value);
}

bool QnGeneralPreferencesWidget::restoreUserSessionData() const
{
    return ui->restoreSessionCheckBox->isChecked();
}

void QnGeneralPreferencesWidget::setRestoreUserSessionData(bool value)
{
    ui->restoreSessionCheckBox->setChecked(value);
}

bool QnGeneralPreferencesWidget::allowComputerEnteringSleepMode() const
{
    return ui->allowEnteringSleepModeCheckBox->isChecked();
}

void QnGeneralPreferencesWidget::setAllowComputerEnteringSleepMode(bool value)
{
    ui->allowEnteringSleepModeCheckBox->setChecked(value);
}

bool QnGeneralPreferencesWidget::muteOnAudioTransmit() const
{
    return ui->muteOnAudioTransmitCheckBox->isChecked();
}

void QnGeneralPreferencesWidget::setMuteOnAudioTransmit(bool value)
{
    ui->muteOnAudioTransmitCheckBox->setChecked(value);
}
