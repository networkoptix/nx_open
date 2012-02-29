#include "workbench_action_handler.h"

#include <cassert>

#include <QFileDialog>
#include <QMessageBox>

#include <utils/common/environment.h>
#include <utils/common/delete_later.h>

#include <core/resourcemanagment/resource_discovery_manager.h>
#include <core/resourcemanagment/resource_pool.h>

#include <api/SessionManager.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>

#include <ui/dialogs/aboutdialog.h>
#include <ui/dialogs/logindialog.h>
#include <ui/dialogs/tagseditdialog.h>
#include <ui/dialogs/serversettingsdialog.h>
#include <ui/dialogs/camerasettingsdialog.h>
#include <ui/dialogs/connectiontestingdialog.h>
#include <ui/dialogs/multiplecamerasettingsdialog.h>
#include <ui/dialogs/layout_save_dialog.h>
#include <ui/dialogs/new_user_dialog.h>
#include <ui/dialogs/new_layout_dialog.h>
#include <ui/preferences/preferencesdialog.h>
#include <youtube/youtubeuploaddialog.h>

#include "eventmanager.h"
#include "file_processor.h"

#include "workbench.h"
#include "workbench_synchronizer.h"
#include "workbench_layout.h"
#include "workbench_item.h"

namespace {
    struct LayoutIdCmp {
        bool operator()(const QnLayoutResourcePtr &l, const QnLayoutResourcePtr &r) const {
            return l->getId() < r->getId();
        }
    };

    enum RemovedResourceType {
        Layout,
        User,
        Server,
        Camera,
        Other,
        Count
    };

    RemovedResourceType removedResourceType(const QnResourcePtr &resource) {
        if(resource->checkFlags(QnResource::layout)) {
            return Layout;
        } else if(resource->checkFlags(QnResource::user)) {
            return User;
        } else if(resource->checkFlags(QnResource::server)) {
            return Server;
        } else if(resource->checkFlags(QnResource::live_cam)) {
            return Camera;
        } else {
            qnWarning("Getting removal type for an unrecognized resource '%1' of type '%2'", resource->getName(), resource->metaObject()->className());
            return Other;
        }
    }

} // anonymous namespace


QnWorkbenchActionHandler::QnWorkbenchActionHandler(QObject *parent):
    QObject(parent),
    m_workbench(NULL),
    m_synchronizer(NULL),
    m_connection(QnAppServerConnectionFactory::createConnection())
{}

QnWorkbenchActionHandler::~QnWorkbenchActionHandler() {
    setWorkbench(NULL);
}

void QnWorkbenchActionHandler::setWorkbench(QnWorkbench *workbench) {
    if(m_workbench != NULL) {
        disconnect(m_workbench, NULL, this, NULL);

        if(m_synchronizer != NULL)
            deinitialize();
    }

    m_workbench = workbench;

    if(m_workbench != NULL) {
        connect(m_workbench, SIGNAL(aboutToBeDestroyed()), this, SLOT(at_workbench_aboutToBeDestroyed()));

        if(m_synchronizer != NULL)
            initialize();
    }
}

void QnWorkbenchActionHandler::setSynchronizer(QnWorkbenchSynchronizer *synchronizer) {
    if(m_synchronizer != NULL) {
        disconnect(m_synchronizer, NULL, this, NULL);

        if(m_workbench != NULL)
            deinitialize();
    }

    m_synchronizer = synchronizer;

    if(m_synchronizer != NULL) {
        connect(m_synchronizer, SIGNAL(destroyed()), this, SLOT(at_synchronizer_destroyed()));

        if(m_workbench != NULL)
            initialize();
    }
}

void QnWorkbenchActionHandler::initialize() {
    assert(m_workbench != NULL);

    /* We're using queued connection here as modifying a field in its change notification handler may lead to problems. */
    connect(m_workbench,                                    SIGNAL(layoutsChanged()), this, SLOT(at_workbench_layoutsChanged()), Qt::QueuedConnection);

    connect(qnAction(Qn::AboutAction),                      SIGNAL(triggered()),    this,   SLOT(at_aboutAction_triggered()));
    connect(qnAction(Qn::SystemSettingsAction),             SIGNAL(triggered()),    this,   SLOT(at_systemSettingsAction_triggered()));
    connect(qnAction(Qn::OpenFileAction),                   SIGNAL(triggered()),    this,   SLOT(at_openFileAction_triggered()));
    connect(qnAction(Qn::OpenFolderAction),                 SIGNAL(triggered()),    this,   SLOT(at_openFolderAction_triggered()));
    connect(qnAction(Qn::ConnectionSettingsAction),         SIGNAL(triggered()),    this,   SLOT(at_connectionSettingsAction_triggered()));
    connect(qnAction(Qn::OpenLayoutAction),                 SIGNAL(triggered()),    this,   SLOT(at_openLayoutAction_triggered()));
    connect(qnAction(Qn::OpenNewLayoutAction),              SIGNAL(triggered()),    this,   SLOT(at_openNewLayoutAction_triggered()));
    connect(qnAction(Qn::OpenSingleLayoutAction),           SIGNAL(triggered()),    this,   SLOT(at_openSingleLayoutAction_triggered()));
    connect(qnAction(Qn::CloseLayoutAction),                SIGNAL(triggered()),    this,   SLOT(at_closeLayoutAction_triggered()));
    connect(qnAction(Qn::CameraSettingsAction),             SIGNAL(triggered()),    this,   SLOT(at_cameraSettingsAction_triggered()));
    connect(qnAction(Qn::MultipleCameraSettingsAction),     SIGNAL(triggered()),    this,   SLOT(at_multipleCamerasSettingsAction_triggered()));
    connect(qnAction(Qn::ServerSettingsAction),             SIGNAL(triggered()),    this,   SLOT(at_serverSettingsAction_triggered()));
    connect(qnAction(Qn::YouTubeUploadAction),              SIGNAL(triggered()),    this,   SLOT(at_youtubeUploadAction_triggered()));
    connect(qnAction(Qn::EditTagsAction),                   SIGNAL(triggered()),    this,   SLOT(at_editTagsAction_triggered()));
    connect(qnAction(Qn::OpenInFolderAction),               SIGNAL(triggered()),    this,   SLOT(at_openInFolderAction_triggered()));
    connect(qnAction(Qn::DeleteFromDiskAction),             SIGNAL(triggered()),    this,   SLOT(at_deleteFromDiskAction_triggered()));
    connect(qnAction(Qn::RemoveLayoutItemAction),           SIGNAL(triggered()),    this,   SLOT(at_removeLayoutItemAction_triggered()));
    connect(qnAction(Qn::RemoveFromServerAction),           SIGNAL(triggered()),    this,   SLOT(at_removeFromServerAction_triggered()));
    connect(qnAction(Qn::NewUserAction),                    SIGNAL(triggered()),    this,   SLOT(at_newUserAction_triggered()));
    connect(qnAction(Qn::NewLayoutAction),                  SIGNAL(triggered()),    this,   SLOT(at_newLayoutAction_triggered()));
    connect(qnAction(Qn::ResourceDropAction),               SIGNAL(triggered()),    this,   SLOT(at_resourceDropAction_triggered()));
}

void QnWorkbenchActionHandler::deinitialize() {
    assert(m_workbench != NULL);

    disconnect(m_workbench, NULL, this, NULL);

    foreach(QAction *action, qnMenu->actions())
        disconnect(action, NULL, this, NULL);
}

QString QnWorkbenchActionHandler::newLayoutName() const {
    const QString zeroName = tr("New layout");
    const QString nonZeroName = tr("New layout %1");
    QRegExp pattern = QRegExp(tr("New layout ?([0-9]+)?"));

    QStringList layoutNames;
    if(m_synchronizer->user()) {
        foreach(const QnLayoutResourcePtr &resource, m_synchronizer->user()->getLayouts())
            layoutNames.push_back(resource->getName());
    } else {
        foreach(QnWorkbenchLayout *layout, workbench()->layouts())
            layoutNames.push_back(layout->name());
    }

    /* Prepare name for new layout. */
    int layoutNumber = -1;
    foreach(const QString &name, layoutNames) {
        if(!pattern.exactMatch(name))
            continue;

        layoutNumber = qMax(layoutNumber, pattern.cap(1).toInt());
    }
    layoutNumber++;

    return layoutNumber == 0 ? zeroName : nonZeroName.arg(layoutNumber);
}

bool QnWorkbenchActionHandler::canAutoDelete(const QnResourcePtr &resource) const {
    if(!resource->checkFlags(QnResource::layout))
        return false;

    QnLayoutResourcePtr layout = resource.staticCast<QnLayoutResource>();
    return m_synchronizer->isLocal(layout) && !m_synchronizer->isChanged(layout);
}

void QnWorkbenchActionHandler::addToWorkbench(const QnResourceList &resources) const {
    foreach(const QnResourcePtr &resource, resources) {
        //if(resource) // TODO

        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid());
        item->setFlag(QnWorkbenchItem::PendingGeometryAdjustment);
        workbench()->currentLayout()->addItem(item);
    }
}


void QnWorkbenchActionHandler::addToWorkbench(const QList<QString> &files) const {
    addToWorkbench(QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(files)));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchActionHandler::at_workbench_aboutToBeDestroyed() {
    setWorkbench(NULL);
}

void QnWorkbenchActionHandler::at_workbench_layoutsChanged() {
    if(!workbench()->layouts().empty())
        return;

    qnAction(Qn::OpenSingleLayoutAction)->trigger();
}

void QnWorkbenchActionHandler::at_synchronizer_destroyed() {
    setSynchronizer(NULL);
}

void QnWorkbenchActionHandler::at_openLayoutAction_triggered() {
    QnLayoutResourcePtr resource = qnMenu->currentResourceTarget(sender()).dynamicCast<QnLayoutResource>();
    if(!resource)
        return;

    QnWorkbenchLayout *layout = QnWorkbenchLayout::layout(resource);
    if(layout == NULL) {
        layout = new QnWorkbenchLayout(resource, workbench());
        workbench()->addLayout(layout);
    }
    workbench()->setCurrentLayout(layout);
}

void QnWorkbenchActionHandler::at_openNewLayoutAction_triggered() {
    QnWorkbenchLayout *layout = new QnWorkbenchLayout(this);
    layout->setName(newLayoutName());

    workbench()->addLayout(layout);
    workbench()->setCurrentLayout(workbench()->layouts().back());
}

void QnWorkbenchActionHandler::at_openSingleLayoutAction_triggered() {
    /* No layouts are open, so we need to open one. */
    QnLayoutResourceList resources = m_synchronizer->user()->getLayouts();
    if(resources.isEmpty()) {
        /* There are no layouts in user's layouts list, just create a new one. */
        qnAction(Qn::OpenNewLayoutAction)->trigger();
    } else {
        /* Open the last layout from the user's layouts list. */
        qSort(resources.begin(), resources.end(), LayoutIdCmp());
        workbench()->addLayout(new QnWorkbenchLayout(resources.back(), this));
        workbench()->setCurrentLayout(workbench()->layouts().back());
    }
}

void QnWorkbenchActionHandler::at_closeLayoutAction_triggered() {
    QnWorkbenchLayoutList layouts = qnMenu->currentLayoutsTarget(sender());
    if(layouts.empty())
        return;
    QnWorkbenchLayout *layout = layouts[0];

    QnLayoutResourcePtr resource = layout->resource();
    bool isChanged = m_synchronizer->isChanged(layout);
    bool isLocal = m_synchronizer->isLocal(layout);

    bool close = false;
    if(isChanged) {
        QScopedPointer<QnLayoutSaveDialog> dialog(new QnLayoutSaveDialog(widget()));
        dialog->setLayoutName(layout->name());
        dialog->exec();
        QDialogButtonBox::StandardButton button = dialog->clickedButton();
        if(button == QDialogButtonBox::Cancel) {
            return;
        } else if(button == QDialogButtonBox::No) {
            m_synchronizer->restore(layout);
            close = true;
        } else {
            layout->setName(dialog->layoutName());
            m_synchronizer->save(layout, this, SLOT(at_layout_saved(int, const QByteArray &, const QnLayoutResourcePtr &)));
            isLocal = false;
            close = true;
        }
    } else {
        close = true;
    }

    if(close) {
        delete layout;

        if(isLocal) {
            m_synchronizer->user()->removeLayout(resource); // TODO
            qnResPool->removeResource(resource);
        }
    }
}

void QnWorkbenchActionHandler::at_resourceDropAction_triggered() {
    addToWorkbench(qnMenu->currentResourcesTarget(sender()));
}

void QnWorkbenchActionHandler::at_openFileAction_triggered() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(widget(), tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::ExistingFiles);
    QStringList filters;
    filters << tr("All Supported (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp *.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("Video (*.mkv *.mp4 *.mov *.ts *.m2ts *.mpeg *.mpg *.flv *.wmv *.3gp)");
    filters << tr("Pictures (*.jpg *.png *.gif *.bmp *.tiff)");
    filters << tr("All files (*.*)");
    dialog->setNameFilters(filters);
    
    if(dialog->exec())
        addToWorkbench(dialog->selectedFiles());
}

void QnWorkbenchActionHandler::at_openFolderAction_triggered() {
    QScopedPointer<QFileDialog> dialog(new QFileDialog(widget(), tr("Open file")));
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOptions(QFileDialog::ShowDirsOnly);
    
    if(dialog->exec())
        addToWorkbench(dialog->selectedFiles());
}

void QnWorkbenchActionHandler::at_aboutAction_triggered() {
    QScopedPointer<AboutDialog> dialog(new AboutDialog(widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_systemSettingsAction_triggered() {
    QScopedPointer<PreferencesDialog> dialog(new PreferencesDialog(widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_connectionSettingsAction_triggered() {
    static LoginDialog *dialog = 0;
    if (dialog)
        return;

    const QUrl lastUsedUrl = qnSettings->lastUsedConnection().url;
    if (lastUsedUrl.isValid() && lastUsedUrl != QnAppServerConnectionFactory::defaultUrl())
        return;

    dialog = new LoginDialog(widget());
    dialog->setModal(true);
    if (dialog->exec()) {
        const QnSettings::ConnectionData connection = qnSettings->lastUsedConnection();
        if (connection.url.isValid()) {
            QnEventManager::instance()->stop();
            SessionManager::instance()->stop();

            QnAppServerConnectionFactory::setDefaultUrl(connection.url);

            // repopulate the resource pool
            QnResource::stopCommandProc();
            QnResourceDiscoveryManager::instance().stop();

            // don't remove local resources
            const QnResourceList remoteResources = qnResPool->getResourcesWithFlag(QnResource::remote);
            qnResPool->removeResources(remoteResources);

            SessionManager::instance()->start();
            QnEventManager::instance()->run();

            QnResourceDiscoveryManager::instance().start();
            QnResource::startCommandProc();
        }
    }
    delete dialog;
    dialog = 0;
}

void QnWorkbenchActionHandler::at_editTagsAction_triggered() {
    QnResourcePtr resource = qnMenu->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QScopedPointer<TagsEditDialog> dialog(new TagsEditDialog(QStringList() << resource->getUniqueId(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_cameraSettingsAction_triggered() {
    QnResourcePtr resource = qnMenu->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QScopedPointer<CameraSettingsDialog> dialog(new CameraSettingsDialog(resource.dynamicCast<QnVirtualCameraResource>(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_multipleCamerasSettingsAction_triggered() {
    QnVirtualCameraResourceList cameras;
    foreach(QnResourcePtr resource, qnMenu->currentResourcesTarget(sender())) {
        QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (camera)
            cameras.append(camera);
    }
    if(cameras.empty())
        return;

    QScopedPointer<MultipleCameraSettingsDialog> dialog(new MultipleCameraSettingsDialog(widget(), cameras));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_serverSettingsAction_triggered() {
    QnResourcePtr resource = qnMenu->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QScopedPointer<ServerSettingsDialog> dialog(new ServerSettingsDialog(resource.dynamicCast<QnVideoServerResource>(), widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_youtubeUploadAction_triggered() {
    QnResourcePtr resource = qnMenu->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QScopedPointer<YouTubeUploadDialog> dialog(new YouTubeUploadDialog(resource, widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->exec();
}

void QnWorkbenchActionHandler::at_openInFolderAction_triggered() {
    QnResourcePtr resource = qnMenu->currentResourceTarget(sender());
    if(resource.isNull())
        return;

    QnEnvironment::showInGraphicalShell(widget(), resource->getUrl());
}

void QnWorkbenchActionHandler::at_deleteFromDiskAction_triggered() {
    QSet<QnResourcePtr> resources = qnMenu->currentResourcesTarget(sender()).toSet();

    QMessageBox::StandardButton button = QMessageBox::question(widget(), tr("Delete Files"), tr("Are you sure you want to permanently delete these %n file(s)?", 0, resources.size()), QMessageBox::Yes | QMessageBox::No);
    if(button != QMessageBox::Yes)
        return;
    
    foreach(const QnResourcePtr &resource, resources)
        QnFileProcessor::deleteLocalResources(resources.toList());
}

void QnWorkbenchActionHandler::at_removeLayoutItemAction_triggered() {
    QnLayoutItemIndexList items = qnMenu->currentLayoutItemsTarget(sender());

    if(items.size() > 1) {
        QMessageBox::StandardButton button = QMessageBox::question(widget(), tr("Remove Items"), tr("Are you sure you want to remove these %n item(s) from layout?", 0, items.size()), QMessageBox::Yes | QMessageBox::No);
        if(button != QMessageBox::Yes)
            return;
    }

    QList<QUuid> orphanedUuids;
    foreach(const QnLayoutItemIndex &index, items) {
        if(index.layout()) {
            index.layout()->removeItem(index.uuid());
        } else {
            orphanedUuids.push_back(index.uuid());
        }
    }

    /* If appserver is not running, we may get removal requests without layout resource. */
    if(!orphanedUuids.isEmpty()) {
        QList<QnWorkbenchLayout *> layouts;
        layouts.push_front(workbench()->currentLayout());
        foreach(const QUuid &uuid, orphanedUuids) {
            foreach(QnWorkbenchLayout *layout, layouts) {
                if(QnWorkbenchItem *item = layout->item(uuid)) {
                    qnDeleteLater(item);
                    break;
                }
            }
        }
    }
}

void QnWorkbenchActionHandler::at_removeFromServerAction_triggered() {
    QnResourceList resources = qnMenu->currentResourcesTarget(sender());
    
    /* User cannot delete himself. */
    resources.removeOne(m_synchronizer->user());
    if(resources.isEmpty())
        return;

    /* Check if it's OK to delete everything without asking. */
    bool okToDelete = true;
    foreach(const QnResourcePtr &resource, resources) {
        if(!canAutoDelete(resource)) {
            okToDelete = false;
            break;
        }
    }

    /* Ask if needed. */
    if(!okToDelete) {
        QString message;

        if(resources.size() == 1) {
            switch(removedResourceType(resources[0])) {
            case Layout:    message = tr("Do you really want to delete layout '%1'?");      break;
            case User:      message = tr("Do you really want to delete user '%1'?");        break;
            case Server:    message = tr("Do you really want to delete server '%1'?");      break;
            case Camera:    message = tr("Do you really want to delete camera '%1'?");      break;
            default:        message = tr("Do you really want to delete resource '%1'?");    break;
            }
            message = message.arg(resources[0]->getName());
        } else {
            message = tr("You are about to delete the following objects:\n\n");

            foreach(const QnResourcePtr &resource, resources) {
                QString subMessage;
                switch(removedResourceType(resource)) {
                case Layout:    subMessage = tr("Layout '%1'");      break;
                case User:      subMessage = tr("User '%1'");        break;
                case Server:    subMessage = tr("Server '%1'");      break;
                case Camera:    subMessage = tr("Camera '%1'");      break;
                default:        subMessage = tr("Resource '%1'");    break;
                }
                message += subMessage.arg(resource->getName()) + QLatin1String("\n");
            }

            message += tr("\nAre you sure you want to delete them?");
        }

        QMessageBox::StandardButton button = QMessageBox::question(widget(), tr("Delete resources"), message, QMessageBox::Yes | QMessageBox::Cancel);
        okToDelete = button == QMessageBox::Yes;
    }

    if(!okToDelete)
        return; /* User does not want it deleted. */

    foreach(const QnResourcePtr &resource, resources) {
        RemovedResourceType type = removedResourceType(resource);

        switch(type) {
        case Layout: {
            QnLayoutResourcePtr layout = resource.staticCast<QnLayoutResource>();
            if(m_synchronizer->isLocal(layout)) {
                m_synchronizer->user()->removeLayout(layout); // TODO
                qnResPool->removeResource(resource);
                break;
            }
            /* FALL THROUGH */
        }
        default: {
            QnAppServerConnectionPtr connection = QnAppServerConnectionFactory::createConnection();
            connection->deleteAsync(resource, this, SLOT(at_resource_deleted(int, const QByteArray &, const QByteArray &, int)));
        }
        }
    }
}

void QnWorkbenchActionHandler::at_newUserAction_triggered() {
    QScopedPointer<QnNewUserDialog> dialog(new QnNewUserDialog(widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    if(!dialog->exec())
        return;

    QnUserResourcePtr user(new QnUserResource());
    user->setName(dialog->login());
    user->setPassword(dialog->password());
    user->setAdmin(dialog->isAdmin());

    m_connection->saveAsync(user, this, SLOT(at_user_saved(int, const QByteArray &, QnResourceList, int)));
}

void QnWorkbenchActionHandler::at_newLayoutAction_triggered() {
    QnUserResourcePtr user = qnMenu->currentResourceTarget(sender()).dynamicCast<QnUserResource>();
    if(user.isNull())
        return;

    QScopedPointer<QnNewLayoutDialog> dialog(new QnNewLayoutDialog(widget()));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setName(newLayoutName());
    if(!dialog->exec())
        return;

    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setGuid(QUuid::createUuid());
    qnResPool->addResource(layout);
    
    layout->setName(dialog->name());
    user->addLayout(layout);
}

void QnWorkbenchActionHandler::at_user_saved(int status, const QByteArray &errorString, const QnResourceList &resources, int handle) {
    if(status == 0) {
        qnResPool->addResources(resources);
        return;
    }

    if(resources.isEmpty() || resources[0].isNull()) {
        QMessageBox::critical(widget(), tr(""), tr("Could not save user to application server. \n\nError description: '%1'").arg(QLatin1String(errorString.data())));
    } else {
        QMessageBox::critical(widget(), tr(""), tr("Could not save user '%1' to application server. \n\nError description: '%2'").arg(resources[0]->getName()).arg(QLatin1String(errorString.data())));
    }
}

void QnWorkbenchActionHandler::at_resource_deleted(int status, const QByteArray &data, const QByteArray &errorString, int handle) {
    if(status == 0)   
        return;

    QMessageBox::critical(widget(), tr(""), tr("Could not delete resource from application server. \n\nError description: '%2'").arg(QLatin1String(errorString.data())));
}

void QnWorkbenchActionHandler::at_layout_saved(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource) {
    if(status == 0)   
        return;

    QMessageBox::critical(widget(), tr(""), tr("Could not save layout '%1' to application server. \n\nError description: '%2'").arg(resource->getName()).arg(QLatin1String(errorString.data())));
}

