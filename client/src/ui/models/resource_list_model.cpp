#include "resource_list_model.h"

#include <utils/common/checked_cast.h>
#include <core/resource/resource.h>

#include <ui/style/resource_icon_cache.h>

QnResourceListModel::QnResourceListModel(QObject *parent): 
    QAbstractListModel(parent)
{}

QnResourceListModel::~QnResourceListModel() {
    return;
}

const QnResourceList &QnResourceListModel::resouces() const {
    return m_resources;
}

void QnResourceListModel::setResources(const QnResourceList &resouces) {
    beginResetModel();

    foreach(const QnResourcePtr &resource, m_resources)
        disconnect(resource.data(), NULL, this, NULL);

    m_resources = resouces;

    foreach(const QnResourcePtr &resource, m_resources) {
        connect(resource.data(), SIGNAL(nameChanged()),                                         this, SLOT(at_resource_resourceChanged()));
        connect(resource.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(at_resource_resourceChanged()));
		connect(resource.data(), SIGNAL(disabledChanged()),                                     this, SLOT(at_resource_resourceChanged()));
		connect(resource.data(), SIGNAL(resourceChanged()),                                     this, SLOT(at_resource_resourceChanged()));
    }

    endResetModel();
}

int QnResourceListModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_resources.size();

    return 0;
}

QVariant QnResourceListModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid())
        return QVariant();

    if(!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnResourcePtr &resource = m_resources[index.row()];
    if(!resource)
        return QVariant();

    switch(role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        return resource->getName();
    case Qt::DecorationRole:
        if (index.column() == 0)
            return qnResIconCache->icon(resource->flags(), resource->getStatus());
        break;
    case Qn::ResourceRole:
        return QVariant::fromValue<QnResourcePtr>(resource);
    case Qn::ResourceFlagsRole:
        return static_cast<int>(resource->flags());
    case Qn::SearchStringRole: 
        return resource->toSearchString();
    case Qn::StatusRole: 
        return static_cast<int>(resource->getStatus());
    default:
        break;
    }

    return QVariant();
}

void QnResourceListModel::at_resource_resourceChanged(const QnResourcePtr &resource) {
    int row = m_resources.indexOf(resource);
    if(row == -1)
        return;

    QModelIndex index = this->index(row, 0);
    emit dataChanged(index, index);
}

void QnResourceListModel::at_resource_resourceChanged() {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    at_resource_resourceChanged(toSharedPointer(checked_cast<QnResource *>(sender)));
}

