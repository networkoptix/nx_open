#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <core/resource_management/resource_pool.h>
#include <ui/models/server_updates_model.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <utils/media_server_update_tool.h>

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

    connect(m_updateTool,       &QnMediaServerUpdateTool::updatesListUpdated,   this,       &QnServerUpdatesWidget::at_updateTool_updatesListUpdated);
}

QnServerUpdatesWidget::~QnServerUpdatesWidget() {}

void QnServerUpdatesWidget::at_checkForUpdatesButton_clicked() {
    m_updateTool->checkForUpdates();
}

void QnServerUpdatesWidget::at_installSpecificBuildButton_clicked() {

}

void QnServerUpdatesWidget::at_updateFromLocalSourceButton_clicked() {
    QString sourceDir = QnFileDialog::getExistingDirectory(this, tr("Select updates folder..."), QString(), QnCustomFileDialog::directoryDialogOptions());
    if (sourceDir.isEmpty())
        return;

    m_updateTool->setLocalUpdateDir(QDir(sourceDir));
    m_updateTool->setUpdateMode(QnMediaServerUpdateTool::LocalUpdate);
    m_updateTool->checkForUpdates();
}

void QnServerUpdatesWidget::at_updateTool_updatesListUpdated() {
    QHash<QnSystemInformation, QnMediaServerUpdateTool::UpdateInformation> updates = m_updateTool->availableUpdates();
    QHash<QnSystemInformation, QnSoftwareVersion> modelUpdates;
    for (auto it = updates.begin(); it != updates.end(); ++it)
        modelUpdates.insert(it.key(), it.value().version);
    m_updatesModel->setUpdates(modelUpdates);
}
