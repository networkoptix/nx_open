#include "resource_list_model.h"

#include <utils/common/checked_cast.h>
#include <core/resource/resource.h>

#include <ui/style/resource_icon_cache.h>

QnResourceListModel::QnResourceListModel(QObject *parent): 
    QAbstractListModel(parent),
    m_readOnly(true)
{}

QnResourceListModel::~QnResourceListModel() {
    return;
}

bool QnResourceListModel::isReadOnly() const {
    return m_readOnly;
}

void QnResourceListModel::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
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

void QnResourceListModel::updateFromResources() {
    QStringList names = m_names;
    m_names.clear();

    if(!names.empty())
        emit dataChanged(index(0, 0), index(names.size() - 1, 0));
}

void QnResourceListModel::submitToResources() {
    for(int i = 0; i < m_names.size(); i++)
        if(!m_names[i].isNull())
            m_resources[i]->setName(m_names[i]);
    m_names.clear();
}

int QnResourceListModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return m_resources.size();

    return 0;
}

Qt::ItemFlags QnResourceListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags result = base_type::flags(index);
    if(m_readOnly)
        return result;

    if(!index.isValid())
        return result;

    if(!hasIndex(index.row(), index.column(), index.parent()))
        return result;

    const QnResourcePtr &resource = m_resources[index.row()];
    if(!resource)
        return result;

    return base_type::flags(index) | Qt::ItemIsEditable;
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
    case Qt::EditRole:
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
        if(m_names.size() > index.row() && !m_names[index.row()].isNull()) {
            return m_names[index.row()];
        } else {
            return resource->getName();
        }
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

bool QnResourceListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(role != Qt::EditRole)
        return false;

    if(!index.isValid())
        return false;

    if(!hasIndex(index.row(), index.column(), index.parent()))
        return false;

    const QnResourcePtr &resource = m_resources[index.row()];
    if(!resource)
        return false;

    while(m_names.size() <= index.row())
        m_names.push_back(QString());

    QString string = value.toString();
    if(string.isEmpty())
        string = QString();

    m_names[index.row()] = string;
    emit dataChanged(index, index);
    
    return true;
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

