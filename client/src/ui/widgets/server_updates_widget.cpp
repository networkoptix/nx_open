#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <QtWidgets/QMessageBox>

#include <core/resource_management/resource_pool.h>
#include <ui/models/server_updates_model.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/build_number_dialog.h>
#include <utils/media_server_update_tool.h>

#include <version.h>

QnServerUpdatesWidget::QnServerUpdatesWidget(QnWorkbenchContext *context, QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::QnServerUpdatesWidget)
{
    ui->setupUi(this);

    m_updateTool = new QnMediaServerUpdateTool(this);

    QList<QnMediaServerResourcePtr> servers;
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server))
        servers.append(resource.staticCast<QnMediaServerResource>());

    m_updatesModel = new QnServerUpdatesModel(this);
    m_updatesModel->setServers(servers);

    ui->tableView->setModel(m_updatesModel);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::ResourceNameColumn, QHeaderView::Stretch);

    connect(ui->updateFromLocalSourceButton,        &QPushButton::clicked,      this,       &QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked);
    connect(ui->checkForUpdatesButton,              &QPushButton::clicked,      this,       &QnServerUpdatesWidget::at_checkForUpdatesButton_clicked);
    connect(ui->installSpecificBuildButton,         &QPushButton::clicked,      this,       &QnServerUpdatesWidget::at_installSpecificBuildButton_clicked);

    connect(m_updateTool,       &QnMediaServerUpdateTool::stateChanged,         this,       &QnServerUpdatesWidget::updateUi);

    updateUi();
    ui->noUpdatesLabel->hide(); // hide it for the first time only
}

QnServerUpdatesWidget::~QnServerUpdatesWidget() {}

void QnServerUpdatesWidget::at_checkForUpdatesButton_clicked() {
    m_updateTool->checkForUpdates();
}

void QnServerUpdatesWidget::at_installSpecificBuildButton_clicked() {
    QnBuildNumberDialog dialog(this);
    if (dialog.exec() == QDialog::Rejected)
        return;

    QnSoftwareVersion version(lit(QN_APPLICATION_VERSION));
    m_updateTool->checkForUpdates(QnSoftwareVersion(version.major(), version.minor(), version.bugfix(), dialog.buildNumber()));
}

void QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked() {
    QString sourceDir = QnFileDialog::getExistingDirectory(this, tr("Select updates folder..."), QString(), QnCustomFileDialog::directoryDialogOptions());
    if (sourceDir.isEmpty())
        return;

    m_updateTool->checkForUpdates(sourceDir);
}

void QnServerUpdatesWidget::updateUpdatesList() {
    QHash<QnSystemInformation, QnMediaServerUpdateTool::UpdateInformation> updates = m_updateTool->availableUpdates();
    QHash<QnSystemInformation, QnSoftwareVersion> modelUpdates;
    for (auto it = updates.begin(); it != updates.end(); ++it)
        modelUpdates.insert(it.key(), it.value().version);
    m_updatesModel->setUpdates(modelUpdates);
}

void QnServerUpdatesWidget::updateUi() {
    bool checkingForUpdates = false;
    bool updateFound = false;
    bool applying = false;
    bool cancellable = false;

    switch (m_updateTool->state()) {
    case QnMediaServerUpdateTool::Idle:
        switch (m_updateTool->updateCheckResult()) {
        case QnMediaServerUpdateTool::UpdateFound:
            updateFound = true;
            break;
        case QnMediaServerUpdateTool::InternetProblem: {
            int result = QMessageBox::warning(this,
                                              tr("Check for updates failed"),
                                              tr("Cannot check for updates via the Internet.\n"
                                                 "Do you want to create a download helper to download updates on the computer which is connected to the internet?"),
                                              QMessageBox::Yes, QMessageBox::No);
            if (result == QDialog::Accepted)
                createUpdatesDownloader();

            ui->noUpdatesLabel->setText(tr("Check for updates failed"));
            break;
        }
        case QnMediaServerUpdateTool::NoNewerVersion:
            ui->noUpdatesLabel->setText(tr("No updates found"));
            break;
        case QnMediaServerUpdateTool::NoSuchBuild:
            QMessageBox::critical(this, tr("Wrong build number"), tr("There is no such build on the update server"));
            ui->noUpdatesLabel->setText(tr("No updates found"));
            break;
        case QnMediaServerUpdateTool::UpdateImpossible:
            ui->noUpdatesLabel->setText(tr("Update is impossible"));
            break;
        }
        updateUpdatesList();
        break;
    case QnMediaServerUpdateTool::CheckingForUpdates:
        checkingForUpdates = true;
        updateUpdatesList();
        break;
    case QnMediaServerUpdateTool::DownloadingUpdate:
        applying = true;
        cancellable = true;
        ui->updateStateLabel->setText(tr("Downloading updates"));
        break;
    case QnMediaServerUpdateTool::UploadingUpdate:
        applying = true;
        cancellable = true;
        ui->updateStateLabel->setText(tr("Uploading updates to servers"));
        break;
    case QnMediaServerUpdateTool::InstallingUpdate:
        applying = true;
        ui->updateStateLabel->setText(tr("Installing updates"));
        break;
    default:
        break;
    }

    if (checkingForUpdates)
        ui->buttonBox->showProgress(tr("Checking for updates..."));
    else
        ui->buttonBox->hideProgress();

    ui->updateButton->setVisible(updateFound);
    ui->cancelButton->setVisible(applying);
    ui->cancelButton->setEnabled(cancellable);
    ui->noUpdatesLabel->setVisible(!ui->updateButton->isVisible());
    ui->updateStateWidget->setVisible(applying);
}

void QnServerUpdatesWidget::createUpdatesDownloader() {

}
