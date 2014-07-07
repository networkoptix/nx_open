#include "system_administration_dialog.h"
#include "ui_system_administration_dialog.h"

#include <QtWidgets/QMessageBox>

#include <ui/widgets/settings/license_manager_widget.h>
#include <ui/widgets/settings/server_settings_widget.h>
#include <ui/widgets/server_updates_widget.h>

QnSystemAdministrationDialog::QnSystemAdministrationDialog(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSystemAdministrationDialog)
{
    ui->setupUi(this);

    m_pages[LicensesPage] = new QnLicenseManagerWidget(this);
    ui->tabWidget->addTab(m_pages[LicensesPage], tr("Licenses"));

    m_pages[ServerPage] = new QnServerSettingsWidget(this);
    ui->tabWidget->addTab(m_pages[ServerPage], tr("Server"));

    m_pages[UpdatesPage] = new QnServerUpdatesWidget(this);
    ui->tabWidget->addTab(m_pages[UpdatesPage], tr("Updates"));
}

void QnSystemAdministrationDialog::reject() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (!page->discard())
            return;

    updateFromSettings();
    base_type::reject();
}

void QnSystemAdministrationDialog::accept() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        if (!page->confirm())
            return;

    submitToSettings();
    base_type::accept();
}

void QnSystemAdministrationDialog::updateFromSettings() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        page->updateFromSettings();
}

void QnSystemAdministrationDialog::submitToSettings() {
    foreach(QnAbstractPreferencesWidget* page, m_pages)
        page->submitToSettings();
}

QnSystemAdministrationDialog::DialogPage QnSystemAdministrationDialog::currentPage() const {
    for (int i = 0; i < PageCount; ++i) {
        DialogPage page = static_cast<DialogPage>(i);
        if (!m_pages.contains(page))
            continue;
        if (ui->tabWidget->currentWidget() == m_pages[page])
            return page;
    }
    return ServerPage;
}

void QnSystemAdministrationDialog::setCurrentPage(DialogPage page) {
    if (!m_pages.contains(page))
        return;
    ui->tabWidget->setCurrentWidget(m_pages[page]);
}
