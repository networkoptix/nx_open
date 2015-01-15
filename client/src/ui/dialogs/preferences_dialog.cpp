#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QtWidgets/QMessageBox>

#include <client/client_settings.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/style/warning_style.h>

#include <ui/widgets/settings/general_preferences_widget.h>
#include <ui/widgets/settings/look_and_feel_preferences_widget.h>
#include <ui/widgets/settings/recording_settings_widget.h>
#include <ui/widgets/settings/popup_settings_widget.h>

#include <ui/workbench/workbench_context.h>

QnPreferencesDialog::QnPreferencesDialog(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PreferencesDialog())
{
    ui->setupUi(this);

    addPage(GeneralPage, new QnGeneralPreferencesWidget(this), tr("General"));
    addPage(LookAndFeelPage, new QnLookAndFeelPreferencesWidget(this), tr("Look and Feel"));

    if (QnScreenRecorder::isSupported()) 
        addPage(RecordingPage, new QnRecordingSettingsWidget(this), tr("Screen Recording"));

    addPage(NotificationsPage, new QnPopupSettingsWidget(this), tr("Notifications"));

    if (qnSettings->isWritable()) {
        ui->readOnlyWarningLabel->hide();
    } else {
        setWarningStyle(ui->readOnlyWarningLabel);
        ui->readOnlyWarningLabel->setText(
#ifdef Q_OS_LINUX
            tr("Settings file is read-only. Please contact your system administrator.\nAll changes will be lost after program exit.")
#else
            tr("Settings cannot be saved. Please contact your system administrator.\nAll changes will be lost after program exit.")
#endif
        );
    }

    resize(1, 1); // set widget size to minimal possible
    loadData();
}

QnPreferencesDialog::~QnPreferencesDialog() {}

void QnPreferencesDialog::submitData() {
    base_type::submitData();
    if (qnSettings->isWritable())
        qnSettings->save();
}

bool QnPreferencesDialog::confirm() {
#ifdef Q_OS_MACX
    // TODO: #dklychkov remove this if the way to restart the app will be found
    return true;
#endif

    bool allPagesConfirm = base_type::confirm();
    if (allPagesConfirm)
        return true;

    QMessageBox::StandardButton result = QMessageBox::information(
        this,
        tr("Information"),
        tr("Some changes will take effect only after application restart. Do you want to restart the application now?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes);
    switch (result) {
    case QMessageBox::Cancel:
        return false;
    case QMessageBox::Yes:
        menu()->trigger(Qn::QueueAppRestartAction);
        break;
    default:
        break;
    }

    return true;
}
