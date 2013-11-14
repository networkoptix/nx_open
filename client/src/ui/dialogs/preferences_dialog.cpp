#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <QtWidgets/QToolButton>
#include <QtWidgets/QFileDialog>


#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/applauncher_utils.h>
#include <client/client_settings.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/style/warning_style.h>

#include <ui/widgets/settings/general_preferences_widget.h>
#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/recording_settings_widget.h>
#include <ui/widgets/settings/popup_settings_widget.h>
#include <ui/widgets/settings/server_settings_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

QnPreferencesDialog::QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::PreferencesDialog()),
    m_generalPreferencesWidget(new QnGeneralPreferencesWidget(this)),
    m_recordingSettingsWidget(NULL),
    m_popupSettingsWidget(new QnPopupSettingsWidget(this)),
    m_licenseManagerWidget(NULL),
    m_serverSettingsWidget(new QnServerSettingsWidget(this))
{
    ui->setupUi(this);

    ui->tabWidget->addTab(m_generalPreferencesWidget, tr("General"));

    if (QnScreenRecorder::isSupported()) {
        m_recordingSettingsWidget = new QnRecordingSettingsWidget(this);
        ui->tabWidget->addTab(m_recordingSettingsWidget, tr("Screen Recorder"));
    }

    ui->tabWidget->addTab(m_popupSettingsWidget, tr("Notifications"));

#ifndef CL_TRIAL_MODE
    m_licenseManagerWidget = new QnLicenseManagerWidget(this);
    ui->tabWidget->addTab(m_licenseManagerWidget, tr("Licenses"));
#endif

    ui->tabWidget->addTab(m_serverSettingsWidget, tr("Server"));

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

    connect(context, SIGNAL(userChanged(const QnUserResourcePtr &)),             this,   SLOT(at_context_userChanged()));
    at_context_userChanged();
}

QnPreferencesDialog::~QnPreferencesDialog() {
}

void QnPreferencesDialog::accept() {

    if (!m_generalPreferencesWidget->confirmCriticalSettings())
        return;

    submitToSettings();
    base_type::accept();
}

void QnPreferencesDialog::submitToSettings() {
    m_generalPreferencesWidget->submitToSettings();

    if (m_recordingSettingsWidget)
        m_recordingSettingsWidget->submitToSettings();

    if (isAdmin()) {
        m_serverSettingsWidget->submitToSettings();
        m_popupSettingsWidget->submitToSettings();
    }

    if (qnSettings->isWritable())
        qnSettings->save();
}

void QnPreferencesDialog::updateFromSettings() {
    m_generalPreferencesWidget->updateFromSettings();
    if (m_recordingSettingsWidget)
        m_recordingSettingsWidget->updateFromSettings();

    if (isAdmin()) {
        m_serverSettingsWidget->updateFromSettings();
        m_popupSettingsWidget->updateFromSettings();
    }
}

bool QnPreferencesDialog::isAdmin() const {
    return accessController()->globalPermissions() & Qn::GlobalProtectedPermission;
}

QnPreferencesDialog::DialogPage QnPreferencesDialog::currentPage() const {
    if (ui->tabWidget->currentWidget() == m_generalPreferencesWidget)
        return GeneralPage;

    if (m_recordingSettingsWidget &&
            ui->tabWidget->currentWidget() == m_recordingSettingsWidget)
        return RecordingPage;

    if (m_licenseManagerWidget &&
            ui->tabWidget->currentWidget() == m_licenseManagerWidget)
        return LicensesPage;

    if (ui->tabWidget->currentWidget() == m_serverSettingsWidget)
        return ServerPage;

    if (ui->tabWidget->currentWidget() == m_popupSettingsWidget)
        return NotificationsPage;

    return GeneralPage;
}

void QnPreferencesDialog::setCurrentPage(QnPreferencesDialog::DialogPage page) {
    switch (page) {
    case GeneralPage:
        ui->tabWidget->setCurrentWidget(m_generalPreferencesWidget);
        break;
    case RecordingPage:
        if (m_recordingSettingsWidget)
            ui->tabWidget->setCurrentWidget(m_recordingSettingsWidget);
        break;
    case LicensesPage:
        if (m_licenseManagerWidget)
            ui->tabWidget->setCurrentWidget(m_licenseManagerWidget);
        break;
    case ServerPage:
        ui->tabWidget->setCurrentWidget(m_serverSettingsWidget);
        m_serverSettingsWidget->updateFocusedElement();
        break;
    case NotificationsPage:
        ui->tabWidget->setCurrentWidget(m_popupSettingsWidget);
        break;
    default:
        break;
    }
}

void QnPreferencesDialog::at_context_userChanged() {
    if (m_licenseManagerWidget)
        ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(m_licenseManagerWidget), isAdmin());
    ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(m_serverSettingsWidget), isAdmin());
    updateFromSettings();
}
