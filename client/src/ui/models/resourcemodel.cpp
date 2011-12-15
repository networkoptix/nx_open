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
        QStandardItem *root = new QStandardItem(QIcon(), server->getName());
        root->setData(server->getId().hash(), Qt::UserRole + 1);
        root->setData(server->toSearchString(), Qt::UserRole + 2);
        root->setEditable(false);
        root->setSelectable(false);
        foreach (QnResourcePtr resource, qnResPool->getResourcesWithParentId(server->getId())) {
            if (!resource->checkFlag(QnResource::server)) {
                QStandardItem *child = new QStandardItem(QIcon(), resource->getName());
                child->setData(resource->getId().hash(), Qt::UserRole + 1);
                child->setData(resource->toSearchString(), Qt::UserRole + 2);
                child->setEditable(false);
                child->setIcon(resource->checkFlag(QnResource::live_cam) ? Skin::icon(QLatin1String("webcam.png")) : Skin::icon(QLatin1String("layout.png")));
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

QStandardItem *ResourceModel::itemFromResourceId(uint id) const
{
    const QModelIndexList indexList = match(index(0, 0), Qt::UserRole + 1, id, 1, Qt::MatchExactly | Qt::MatchRecursive);
    return !indexList.isEmpty() ? itemFromIndex(indexList.first()) : 0;
}

void ResourceModel::addResource(QnResourcePtr resource)
{
    uint parentId = -1;
    if (resource->checkFlag(QnResource::local))
        parentId = 0;
    else if (resource->checkFlag(QnResource::remote))
        parentId = resource->getParentId().hash();

    if (resource->checkFlag(QnResource::server)) {
        QStandardItem *root = new QStandardItem(QIcon(), resource->getName());
        root->setData(resource->getId().hash(), Qt::UserRole + 1);
        root->setData(resource->toSearchString(), Qt::UserRole + 2);
        root->setEditable(false);
        root->setSelectable(false);
        appendRow(root);
    } else if (QStandardItem *root = itemFromResourceId(parentId)) {
        QStandardItem *child = new QStandardItem(QIcon(), resource->getName());
        child->setData(resource->getId().hash(), Qt::UserRole + 1);
        child->setData(resource->toSearchString(), Qt::UserRole + 2);
        child->setEditable(false);
        child->setIcon(resource->checkFlag(QnResource::live_cam) ? Skin::icon(QLatin1String("webcam.png")) : Skin::icon(QLatin1String("layout.png")));
        root->appendRow(child);
    }
}

void ResourceModel::removeResource(QnResourcePtr resource)
{
    if (QStandardItem *child = itemFromResourceId(resource->getId().hash())) {
        foreach (QStandardItem *item, takeRow(child->row()))
            delete item;
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
                QnResourcePtr resource = qnResPool->getResourceById(index.data(Qt::UserRole + 1));
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
    if (mimeData->hasUrls())
        resources += QnFileProcessor::createResourcesForFiles(QnFileProcessor::findAcceptedFiles(mimeData->urls()));
    foreach (const QnResourcePtr &resource, resources) {
        if (!resource->checkFlag(QnResource::local) && !resource->checkFlag(QnResource::server))
            addResource(resource);
    }

    return true;
}
