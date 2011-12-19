#include "resourcemodel.h"

#include <QtCore/QMimeData>

#include "core/resourcemanagment/resource_pool.h"

#include "ui/skin.h"
#include "ui/view_drag_and_drop.h"

#include "file_processor.h"

ResourceModel::ResourceModel(QObject *parent)
    : QStandardItemModel(parent)
{
    foreach (QnResourcePtr server, qnResPool->getResourcesWithFlag(QnResource::server)) {
        QStandardItem *root = new QStandardItem(server->getName());
        root->setData(server->getId().hash(), Qt::UserRole + 1);
        root->setData(server->toSearchString(), Qt::UserRole + 2);
        //root->setIcon(Skin::icon(QLatin1String("server.png")));
        root->setEditable(false);
        root->setSelectable(false);
        foreach (QnResourcePtr resource, qnResPool->getResourcesWithParentId(server->getId())) {
            if (!resource->checkFlag(QnResource::server)) {
                QStandardItem *child = new QStandardItem(resource->getName());
                child->setData(resource->getId().hash(), Qt::UserRole + 1);
                child->setData(resource->toSearchString(), Qt::UserRole + 2);
                child->setIcon(resource->checkFlag(QnResource::live_cam) ? Skin::icon(QLatin1String("webcam.png")) : Skin::icon(QLatin1String("layout.png")));
                child->setEditable(false);
                root->appendRow(child);
            }
        }
        appendRow(root);
    }

    connect(qnResPool, SIGNAL(resourceAdded(QnResourcePtr)), this, SLOT(addResource(QnResourcePtr)));
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(removeResource(QnResourcePtr)));

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

void ResourceModel::addResource(QnResourcePtr resource)
{
    if (sender() != qnResPool && !qnResPool->getResourceById(resource->getId())) {
        qnResPool->addResource(resource); // emits resourceAdded; thus return
        return;
    }

    if (itemFromResource(resource))
        return; // avoid duplicates

    if (resource->checkFlag(QnResource::server)) {
        QStandardItem *root = new QStandardItem(resource->getName());
        root->setData(resource->getId().hash(), Qt::UserRole + 1);
        root->setData(resource->toSearchString(), Qt::UserRole + 2);
        //root->setIcon(Skin::icon(QLatin1String("server.png")));
        root->setEditable(false);
        root->setSelectable(false);
        appendRow(root);
    } else if (QStandardItem *root = itemFromResourceId(resource->getParentId().hash())) {
        QStandardItem *child = new QStandardItem(resource->getName());
        child->setData(resource->getId().hash(), Qt::UserRole + 1);
        child->setData(resource->toSearchString(), Qt::UserRole + 2);
        child->setIcon(resource->checkFlag(QnResource::live_cam) ? Skin::icon(QLatin1String("webcam.png")) : Skin::icon(QLatin1String("layout.png")));
        child->setEditable(false);
        root->appendRow(child);
    } else {
        qWarning("ResourceModel::addResource(): parent resource (id %d) wasn't found for resource (id %d)",
                 resource->getParentId().hash(), resource->getId().hash());
    }
}

void ResourceModel::removeResource(QnResourcePtr resource)
{
    if (sender() != qnResPool && qnResPool->getResourceById(resource->getId())) {
        qnResPool->removeResource(resource); // emits resourceRemoved; thus return
        return;
    }

    if (QStandardItem *item = itemFromResource(resource)) {
        foreach (QStandardItem *rowItem, takeRow(item->row()))
            delete rowItem;
    }
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
