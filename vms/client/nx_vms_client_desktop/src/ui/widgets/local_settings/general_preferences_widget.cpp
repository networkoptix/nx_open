#include "general_preferences_widget.h"
#include "ui_general_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtMultimedia/QAudioDeviceInfo>

#include <client/client_settings.h>

#include <common/common_module.h>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/screen_recording/video_recorder_settings.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <nx/vms/utils/platform/autorun.h>

namespace {
const int kMsecsPerMinute = 60 * 1000;
}

QnGeneralPreferencesWidget::QnGeneralPreferencesWidget(
    QnVideoRecorderSettings* settings,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::GeneralPreferencesWidget),
    m_recorderSettings(settings)
{
    ui->setupUi(this);

    if (!nx::vms::utils::isAutoRunSupported())
        ui->autoStartCheckBox->hide();

    setHelpTopic(ui->mediaFoldersGroupBox, Qn::SystemSettings_General_MediaFolders_Help);
    setHelpTopic(ui->pauseOnInactivityCheckBox, Qn::SystemSettings_General_AutoPause_Help);
    setHelpTopic(ui->idleTimeoutSpinBox, ui->idleTimeoutWidget,
        Qn::SystemSettings_General_AutoPause_Help);
    setHelpTopic(ui->autoStartCheckBox, Qn::SystemSettings_General_AutoStartWithSystem_Help);

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
    connect(ui->primaryAudioDeviceComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->secondaryAudioDeviceComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->playAudioForAllCamerasCheckbox, &QCheckBox::toggled, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

QnGeneralPreferencesWidget::~QnGeneralPreferencesWidget()
{
}

void QnGeneralPreferencesWidget::applyChanges()
{
    QStringList allMediaFolders = mediaFolders();
    QString mainMediaFolder = allMediaFolders.isEmpty()
        ? QString()
        : allMediaFolders.takeFirst();

    qnSettings->setMediaFolder(mainMediaFolder);
    qnSettings->setExtraMediaFolders(allMediaFolders);
    qnSettings->setUserIdleTimeoutMSecs(userIdleTimeoutMs());
    qnSettings->setAutoStart(autoStart());
    qnSettings->setPlayAudioForAllItems(ui->playAudioForAllCamerasCheckbox->isChecked());

    bool recorderSettingsChanged = false;
    if (m_recorderSettings->primaryAudioDeviceName() != primaryAudioDeviceName())
    {
        m_recorderSettings->setPrimaryAudioDeviceName(primaryAudioDeviceName());
        recorderSettingsChanged = true;
    }

    if (m_recorderSettings->secondaryAudioDeviceName() != secondaryAudioDeviceName())
    {
        m_recorderSettings->setSecondaryAudioDeviceName(secondaryAudioDeviceName());
        recorderSettingsChanged = true;
    }

    if (recorderSettingsChanged)
        emit recordingSettingsChanged();

    emit mediaDirectoriesChanged();
}

void QnGeneralPreferencesWidget::loadDataToUi()
{
    QStringList allMediaFolders;
    allMediaFolders << qnSettings->mediaFolder();
    allMediaFolders << qnSettings->extraMediaFolders();
    setMediaFolders(allMediaFolders);
    setUserIdleTimeoutMs(qnSettings->userIdleTimeoutMSecs());
    setPrimaryAudioDeviceName(m_recorderSettings->primaryAudioDeviceName());
    setSecondaryAudioDeviceName(m_recorderSettings->secondaryAudioDeviceName());
    setAutoStart(qnSettings->autoStart());
    ui->playAudioForAllCamerasCheckbox->setChecked(qnSettings->playAudioForAllItems());
}

bool QnGeneralPreferencesWidget::hasChanges() const
{
    if (ui->playAudioForAllCamerasCheckbox->isChecked() != qnSettings->playAudioForAllItems())
        return true;

    QStringList allMediaFolders;
    allMediaFolders << qnSettings->mediaFolder();
    allMediaFolders << qnSettings->extraMediaFolders();
    if (allMediaFolders != mediaFolders())
        return true;

    bool oldPauseOnInactivity = qnSettings->userIdleTimeoutMSecs() > 0;
    bool newPauseOnInactivity = userIdleTimeoutMs() > 0;
    if (oldPauseOnInactivity != newPauseOnInactivity)
        return true;

    /* Do not compare negative values. */
    if (newPauseOnInactivity)
    {
        if (qnSettings->userIdleTimeoutMSecs() != userIdleTimeoutMs())
            return true;
    }

    if (nx::vms::utils::isAutoRunSupported())
    {
        if (qnSettings->autoStart() != autoStart())
            return true;
    }

    if (m_recorderSettings->primaryAudioDeviceName() != primaryAudioDeviceName())
        return true;

    if (m_recorderSettings->secondaryAudioDeviceName() != secondaryAudioDeviceName())
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
        tr("Select folder..."),
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

    QString defaultPrimaryDeviceName = m_recorderSettings->defaultPrimaryAudioDevice().fullName();
    if (defaultPrimaryDeviceName.isEmpty())
        defaultPrimaryDeviceName = tr("None");
    ui->primaryAudioDeviceComboBox->addItem(tr("Auto (%1)").arg(defaultPrimaryDeviceName));
    ui->primaryAudioDeviceComboBox->addItem(tr("None"), QnAudioRecorderSettings::kNoDevice);

    QString defaultSecondaryDeviceName =
        m_recorderSettings->defaultSecondaryAudioDevice().fullName();
    if (defaultSecondaryDeviceName.isEmpty())
        defaultSecondaryDeviceName = tr("None");
    ui->secondaryAudioDeviceComboBox->addItem(tr("Auto (%1)").arg(defaultSecondaryDeviceName));
    ui->secondaryAudioDeviceComboBox->addItem(tr("None"), QnAudioRecorderSettings::kNoDevice);

    for (const auto& deviceName: m_recorderSettings->availableDevices())
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
