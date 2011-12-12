#include "resourcemodel.h"

#include "core/resourcemanagment/resource_pool.h"

#include "ui/skin.h"

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
    if (resource->checkFlag(QnResource::server))
    {
        QStandardItem *root = new QStandardItem(QIcon(), resource->getName());
        root->setData(resource->getId().hash(), Qt::UserRole + 1);
        root->setData(resource->toSearchString(), Qt::UserRole + 2);
        root->setEditable(false);
        root->setSelectable(false);
        appendRow(root);
    }
    else if (QStandardItem *root = itemFromResourceId(parentId))
    {
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
    if (QStandardItem *child = itemFromResourceId(resource->getId().hash()))
    {
        foreach (QStandardItem *item, takeRow(child->row()))
            delete item;
    }
}
