#include "preferences_dialog.h"
#include "ui_preferences_dialog.h"

#include <client/client_settings.h>

#include <api/app_server_connection.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/style/warning_style.h>

#include <ui/widgets/settings/general_preferences_widget.h>
#include <ui/widgets/settings/recording_settings_widget.h>
#include <ui/widgets/settings/popup_settings_widget.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

QnPreferencesDialog::QnPreferencesDialog(QnWorkbenchContext *context, QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::PreferencesDialog())
{
    ui->setupUi(this);

    m_pages[GeneralPage] = new QnGeneralPreferencesWidget(this);
    ui->tabWidget->addTab(m_pages[GeneralPage], tr("General"));

    if (QnScreenRecorder::isSupported()) {
        m_pages[RecordingPage] = new QnRecordingSettingsWidget(this);
        ui->tabWidget->addTab(m_pages[RecordingPage], tr("Screen Recording"));
    }

    m_pages[NotificationsPage] = new QnPopupSettingsWidget(this);
    ui->tabWidget->addTab(m_pages[NotificationsPage], tr("Notifications"));

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
    updateFromSettings();
}

QnPreferencesDialog::~QnPreferencesDialog() {
}

void QnPreferencesDialog::accept() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (!page->confirm())
            return;

    submitToSettings();
    base_type::accept();
}

void QnPreferencesDialog::submitToSettings() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        page->submitToSettings();

    if (qnSettings->isWritable())
        qnSettings->save();
}

void QnPreferencesDialog::updateFromSettings() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        page->updateFromSettings();
}

QnPreferencesDialog::DialogPage QnPreferencesDialog::currentPage() const {
    for (int i = 0; i < PageCount; ++i) {
        DialogPage page = static_cast<DialogPage>(i);
        if (!m_pages.contains(page))
            continue;
        if (ui->tabWidget->currentWidget() == m_pages[page])
            return page;
    }
    return GeneralPage;
}

void QnPreferencesDialog::setCurrentPage(QnPreferencesDialog::DialogPage page) {
    if (!m_pages.contains(page))
        return;
    ui->tabWidget->setCurrentWidget(m_pages[page]);
}
