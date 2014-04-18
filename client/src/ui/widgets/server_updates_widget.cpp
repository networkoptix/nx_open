#include "server_updates_widget.h"
#include "ui_server_updates_widget.h"

#include <core/resource_management/resource_pool.h>
#include <ui/models/server_updates_model.h>

QnServerUpdatesWidget::QnServerUpdatesWidget(QnWorkbenchContext *context, QWidget *parent) :
    QWidget(parent),
    QnWorkbenchContextAware(context),
    ui(new Ui::QnServerUpdatesWidget)
{
    ui->setupUi(this);

    QList<QnMediaServerResourcePtr> servers;
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server))
        servers.append(resource.staticCast<QnMediaServerResource>());

    QnServerUpdatesModel *model = new QnServerUpdatesModel(this);
    model->setServers(servers);

    ui->tableView->setModel(model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnServerUpdatesModel::ResourceNameColumn, QHeaderView::Stretch);

}

QnServerUpdatesWidget::~QnServerUpdatesWidget() {}
