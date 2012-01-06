#include "resourcemodel.h"

#include <QtCore/QMimeData>

#include <core/resourcemanagment/resource_pool.h>

#include "ui/skin/skin.h"

#include "ui/view_drag_and_drop.h"

#include "file_processor.h"

ResourceModel::ResourceModel(QObject *parent)
    : QStandardItemModel(parent)
{
    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), this, SLOT(_q_addResource(QnResourcePtr)));
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(_q_removeResource(QnResourcePtr)));

    const QnResourceList resources = qnResPool->getResources();
    foreach (QnResourcePtr server, resources) {
        if (server->checkFlag(QnResource::server)) {
            addResource(server);
            const QnId serverId = server->getId();
            foreach (QnResourcePtr resource, resources) {
                if (resource->getParentId() == serverId && !resource->checkFlag(QnResource::server))
                    addResource(resource);
            }
        }
    }

    QHash<int, QByteArray> roles = roleNames();
    roles.insert(Qt::UserRole + 1, "id");
    roles.insert(Qt::UserRole + 2, "searchString");
    setRoleNames(roles);
}

ResourceModel::~ResourceModel()
{
}

QnResourcePtr ResourceModel::resourceFromIndex(const QModelIndex &index) const
{
    return qnResPool->getResourceById(index.data(Qt::UserRole + 1));
}

QnResourcePtr ResourceModel::resourceFromItem(QStandardItem *item) const
{
    return resourceFromIndex(item->index());
}

QStandardItem *ResourceModel::itemFromResource(QnResourcePtr resource) const
{
    return itemFromResourceId(resource->getId().hash());
}

QStandardItem *ResourceModel::itemFromResourceId(uint id) const
{
    const QModelIndexList indexList = match(index(0, 0), Qt::UserRole + 1, id, 1, Qt::MatchExactly | Qt::MatchRecursive);
    return !indexList.isEmpty() ? itemFromIndex(indexList.first()) : 0;
}

static inline QIcon iconForResource(QnResourcePtr resource)
{
    QString iconName;
    if (resource->checkFlag(QnResource::server))
        iconName = resource->checkFlag(QnResource::remote) ? QLatin1String("server.png") : QLatin1String("home.png");
    else if (resource->checkFlag(QnResource::layout))
        iconName = QLatin1String("layout.png");
    else if (resource->checkFlag(QnResource::live_cam))
        iconName = QLatin1String("webcam.png");
    else if (resource->checkFlag(QnResource::SINGLE_SHOT))
        iconName = QLatin1String("snapshot.png");
    else if (resource->checkFlag(QnResource::ARCHIVE) || resource->checkFlag(QnResource::server_archive))
        iconName = QLatin1String("media.png");

    return !iconName.isEmpty() ? Skin::icon(iconName) : QIcon();
}

void ResourceModel::addResource(QnResourcePtr resource)
{
    if (resource->checkFlag(QnResource::server)) {
        QStandardItem *root = new QStandardItem(resource->getName());
        root->setData(resource->getId().hash(), Qt::UserRole + 1);
        root->setData(resource->toSearchString(), Qt::UserRole + 2);
        root->setIcon(iconForResource(resource));
        root->setEditable(false);
        root->setSelectable(false);
        appendRow(root);
    } else if (QStandardItem *root = itemFromResourceId(resource->getParentId().hash())) {
        QStandardItem *child = new QStandardItem(resource->getName());
        child->setData(resource->getId().hash(), Qt::UserRole + 1);
        child->setData(resource->toSearchString(), Qt::UserRole + 2);
        child->setIcon(iconForResource(resource));
        child->setEditable(false);
        root->appendRow(child);
    } else {
        qWarning("ResourceModel::addResource(): parent resource (id %d) wasn't found for resource (id %d)",
                 resource->getParentId().hash(), resource->getId().hash());
    }
}

void ResourceModel::removeResource(QnResourcePtr resource)
{
    if (QStandardItem *item = itemFromResource(resource)) {
        foreach (QStandardItem *rowItem, takeRow(item->row()))
            delete rowItem;
    }
}

void ResourceModel::_q_addResource(QnResourcePtr resource)
{
    if (sender() != qnResPool && !qnResPool->getResourceById(resource->getId())) {
        qnResPool->addResource(resource); // emits resourceAdded; thus return
        return;
    }

    if (itemFromResource(resource))
        return; // avoid duplicates

    addResource(resource);
}

void ResourceModel::_q_removeResource(QnResourcePtr resource)
{
    if (sender() != qnResPool && qnResPool->getResourceById(resource->getId())) {
        qnResPool->removeResource(resource); // emits resourceRemoved; thus return
        return;
    }

    removeResource(resource);
}

QStringList ResourceModel::mimeTypes() const
{
    QStringList mimeTypes = QStandardItemModel::mimeTypes();
    mimeTypes.append(QLatin1String("text/uri-list"));
    mimeTypes.append(resourcesMime());

    return mimeTypes;
}

QMimeData *ResourceModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = QStandardItemModel::mimeData(indexes);
    if (mimeData) {
        const QStringList types = mimeTypes();
        const QString resourceFormat = resourcesMime();
        const QString urlFormat = QLatin1String("text/uri-list");
        if (types.contains(resourceFormat) || types.contains(urlFormat)) {
            QnResourceList resources;
            foreach (const QModelIndex &index, indexes) {
                QnResourcePtr resource = resourceFromIndex(index);
                if (resource)
                    resources.append(resource);
            }
            if (types.contains(resourceFormat))
                mimeData->setData(resourceFormat, serializeResources(resources));
            if (types.contains(urlFormat)) {
                QList<QUrl> urls;
                foreach (const QnResourcePtr &resource, resources) {
                    if (resource->checkFlag(QnResource::url))
                        urls.append(QUrl::fromLocalFile(resource->getUrl()));
                }
                mimeData->setUrls(urls);
            }
        }
    }

    return mimeData;
}

bool ResourceModel::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    // check if the action is supported
    if (!mimeData || !(action == Qt::CopyAction || action == Qt::MoveAction))
        return false;

    // check if the format is supported
    const QString format = resourcesMime();
    if (!mimeData->hasFormat(format) && !mimeData->hasUrls())
        return QStandardItemModel::dropMimeData(mimeData, action, row, column, parent);

    // decode and insert
    QnResourceList resources;
    if (!mimeData->hasFormat(format))
       resources += deserializeResources(mimeData->data(format));
    else if (mimeData->hasUrls())
        resources += QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(mimeData->urls()));
    foreach (const QnResourcePtr &resource, resources) {
        if (resource->checkFlag(QnResource::local) && !resource->checkFlag(QnResource::server))
            addResource(resource);
    }

    return true;
}
