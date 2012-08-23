
#if 0

#include "workbench_ecs_handler.h"

#include <api/app_server_connection.h>

#include <ui/dialogs/resource_list_dialog.h>

QnWorkbenchEcsHandler::QnWorkbenchEcsHandler(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{

}

QnWorkbenchEcsHandler::~QnWorkbenchEcsHandler() {
    return;
}

QnAppServerConnectionPtr QnWorkbenchEcsHandler::connection() const {
    return QnAppServerConnectionFactory::createConnection();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchEcsHandler::at_resources_saved(int status, const QByteArray &errorString, const QnResourceList &resources, int handle) {
    Q_UNUSED(handle);

    if(status == 0)
        return;

    QnLayoutResourceList layoutResources = resources.filtered<QnLayoutResource>();
    QnLayoutResourceList reopeningLayoutResources;
    foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
        if(snapshotManager()->isLocal(layoutResource) && !QnWorkbenchLayout::instance(layoutResource))
            reopeningLayoutResources.push_back(layoutResource);

    if(!reopeningLayoutResources.empty()) {
        int button = QnResourceListDialog::exec(
            widget(),
            resources,
            tr("Error"),
            tr("Could not save the following %n layout(s) to Enterprise Controller.", NULL, reopeningLayoutResources.size()),
            tr("Do you want to restore these %n layout(s)?", NULL, reopeningLayoutResources.size()),
            QDialogButtonBox::Yes | QDialogButtonBox::No
        );
        if(button == QMessageBox::Yes) {
            foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
                workbench()->addLayout(new QnWorkbenchLayout(layoutResource, this));
            workbench()->setCurrentLayout(workbench()->layouts().back());
        } else {
            foreach(const QnLayoutResourcePtr &layoutResource, layoutResources)
                resourcePool()->removeResource(layoutResource);
        }
    } else {
        QnResourceListDialog::exec(
            widget(),
            resources,
            tr("Error"),
            tr("Could not save the following %n items to Enterprise Controller.", NULL, resources.size()),
            tr("Error description: \n%1").arg(QLatin1String(errorString.data())),
            QDialogButtonBox::Ok
        );
    }
}

void QnWorkbenchEcsHandler::at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle) {
    Q_UNUSED(handle);
    Q_UNUSED(data);

    if(status == 0)   
        return;

    QMessageBox::critical(widget(), tr(""), tr("Could not delete resource from Enterprise Controller. \n\nError description: '%2'").arg(QLatin1String(errorString.data())));
}

void QnWorkbenchEcsHandler::at_resources_statusSaved(int status, const QByteArray &errorString, const QnResourceList &resources, const QList<int> &oldDisabledFlags) {
    if(status == 0 || resources.isEmpty())
        return;

    QnResourceListDialog::exec(
        widget(),
        resources,
        tr("Error"),
        tr("Could not save changes made to the following %n resource(s).", NULL, resources.size()),
        tr("Error description:\n%1").arg(QLatin1String(errorString.constData())),
        QDialogButtonBox::Ok
    );

    for(int i = 0; i < resources.size(); i++)
        resources[i]->setDisabled(oldDisabledFlags[i]);
}


#endif
