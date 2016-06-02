#include "local_settings_dialog.h"
#include "ui_local_settings_dialog.h"

#include <client/client_settings.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/style/custom_style.h>

#include <ui/widgets/local_settings/general_preferences_widget.h>
#include <ui/widgets/local_settings/look_and_feel_preferences_widget.h>
#include <ui/widgets/local_settings/recording_settings_widget.h>
#include <ui/widgets/local_settings/popup_settings_widget.h>

#include <ui/workbench/workbench_context.h>

QnLocalSettingsDialog::QnLocalSettingsDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LocalSettingsDialog()),
    m_generalWidget(new QnGeneralPreferencesWidget(this)),
    m_lookAndFeelWidget(new QnLookAndFeelPreferencesWidget(this))
{
    ui->setupUi(this);

    addPage(GeneralPage, m_generalWidget , tr("General"));
    addPage(LookAndFeelPage, m_lookAndFeelWidget, tr("Look and Feel"));

    auto recordingSettingsWidget = new QnRecordingSettingsWidget(this);
    bool audioOnlyMode = !QnScreenRecorder::isSupported();
    recordingSettingsWidget->setAudioOnlyMode(audioOnlyMode);

    addPage(
        RecordingPage,
        recordingSettingsWidget,
        audioOnlyMode ? tr("Audio Settings") : tr("Screen Recording"));


    addPage(NotificationsPage, new QnPopupSettingsWidget(this), tr("Notifications"));

    setWarningStyle(ui->readOnlyWarningLabel);
    ui->readOnlyWarningLabel->setVisible(!qnSettings->isWritable());
    ui->readOnlyWarningLabel->setText(
#ifdef Q_OS_LINUX
        tr("Settings file is read-only. Please contact your system administrator. All changes will be lost after program exit.")
#else
        tr("Settings cannot be saved. Please contact your system administrator. All changes will be lost after program exit.")
#endif
    );

    loadDataToUi();
}

QnLocalSettingsDialog::~QnLocalSettingsDialog() {}

void QnLocalSettingsDialog::applyChanges()
{
    bool restartQueued = false;

    // TODO: #dklychkov remove this define if the way to restart the app will be found
#ifndef Q_OS_MACX
    if (m_generalWidget->isRestartRequired() || m_lookAndFeelWidget->isRestartRequired())
    {
        QDialogButtonBox::StandardButton result = QnMessageBox::information(
            this,
            tr("Information"),
            tr("Some changes will take effect only after application restart. Do you want to restart the application now?"),
            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
            QDialogButtonBox::Yes);
        switch (result)
        {
        case QDialogButtonBox::Cancel:
            return;
        case QDialogButtonBox::Yes:
            restartQueued = true;
            break;
        default:
            break;
        }
    }
#endif

    base_type::applyChanges();
    if (qnSettings->isWritable())
        qnSettings->save();

    if (restartQueued)
        menu()->trigger(QnActions::QueueAppRestartAction);
}

