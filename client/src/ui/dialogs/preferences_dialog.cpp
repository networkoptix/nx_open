#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"


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

QnPreferencesDialog::QnPreferencesDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PreferencesDialog()),
    m_generalWidget(new QnGeneralPreferencesWidget(this)),
    m_lookAndFeelWidget(new QnLookAndFeelPreferencesWidget(this))
{
    ui->setupUi(this);

    addPage(GeneralPage, m_generalWidget , tr("General"));
    addPage(LookAndFeelPage, m_lookAndFeelWidget, tr("Look and Feel"));

    if (QnScreenRecorder::isSupported())
        addPage(RecordingPage, new QnRecordingSettingsWidget(this), tr("Screen Recording"));

    addPage(NotificationsPage, new QnPopupSettingsWidget(this), tr("Notifications"));

    if (qnSettings->isWritable()) {
        ui->readOnlyWarningLabel->hide();
    } else {
        setWarningStyle(ui->readOnlyWarningLabel);
        ui->readOnlyWarningLabel->setText(
#ifdef Q_OS_LINUX
            tr("Settings file is read-only. Please contact your system administrator.") + L'\n' + tr("All changes will be lost after program exit.")
#else
            tr("Settings cannot be saved. Please contact your system administrator.") + L'\n' + tr("All changes will be lost after program exit.")
#endif
        );
    }

    loadDataToUi();
}

QnPreferencesDialog::~QnPreferencesDialog() {}

void QnPreferencesDialog::applyChanges()
{
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
            /* The slot must be connected as QueuedConnection because it must start the new instance
            * after the settings have been saved. Settings saving will be performed just after this (confirm)
            * without returning to the event loop. */
            menu()->trigger(QnActions::QueueAppRestartAction);
            break;
        default:
            break;
        }
    }
#endif

    base_type::applyChanges();
    if (qnSettings->isWritable())
        qnSettings->save();
}

