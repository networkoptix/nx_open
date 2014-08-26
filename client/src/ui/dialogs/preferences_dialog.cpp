#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <client/client_settings.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/style/warning_style.h>

#include <ui/widgets/settings/general_preferences_widget.h>
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
