#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

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
        ui->noUpdatesLabel->setText(tr("No updates available"));
        break;
    case QnMediaServerUpdateTool::CheckingForUpdates:
        checkingForUpdates = true;
        break;
    case QnMediaServerUpdateTool::UpdateFound:
        updateFound = true;
        updateUpdatesList();
        break;
    case QnMediaServerUpdateTool::UpdateImpossible:
        ui->noUpdatesLabel->setText(tr("Update is impossible"));
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
