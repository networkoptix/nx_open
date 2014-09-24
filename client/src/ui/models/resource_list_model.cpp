#include "resource_list_model.h"

#include <utils/common/checked_cast.h>
#include <core/resource/resource.h>
#include <core/resource/resource_name.h>

#include <ui/style/resource_icon_cache.h>
#include <client/client_globals.h>

QnResourceListModel::QnResourceListModel(QObject *parent): 
    QAbstractListModel(parent),
    m_readOnly(true), // TODO: #Elric change to false, makes more sense.
    m_showIp(false)
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
        connect(resource.data(), SIGNAL(nameChanged(const QnResourcePtr &)),    this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
        connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),  this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
        connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    }

    endResetModel();
}

void QnResourceListModel::addResource(const QnResourcePtr &resource) {
    foreach(const QnResourcePtr &r, m_resources) // TODO: #Elric checking for duplicates does not belong here. Makes no sense!
        if (r->getId() == resource->getId())
            return;

    beginResetModel();
    connect(resource.data(), SIGNAL(nameChanged(const QnResourcePtr &)),    this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),  this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    m_resources << resource;
    endResetModel();
}

void QnResourceListModel::removeResource(const QnResourcePtr &resource) {
    if (!resource)
        return;

    beginResetModel();

    for (int i = 0; i < m_resources.size(); ++i) {
        if (m_resources[i]->getId() == resource->getId()) { // TODO: #Elric check by pointer, not id. Makes no sense.
            disconnect(m_resources[i].data(), NULL, this, NULL);
            m_resources.removeAt(i);
            break;
        }
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

void QnResourceListModel::setShowIp(bool showIp) {
    m_showIp = showIp;
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

    return result | Qt::ItemIsEditable;
}


QVariant QnResourceListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    const QnResourcePtr &resource = m_resources[index.row()];
    if(!resource)
        return QVariant();

    switch(role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole: {
        QString name;
        if(m_names.size() > index.row() && !m_names[index.row()].isNull()) {
            name = m_names[index.row()];
        } else {
            name = resource->getName();
        }

        if (m_showIp && role != Qt::EditRole)
            return lit("%1 (%2)").arg(name, extractHost(resource->getUrl()));
        else
            return name;
    }
    case Qt::DecorationRole:
        if (index.column() == 0)
            return qnResIconCache->icon(resource);
        break;
    case Qn::ResourceRole:
        return QVariant::fromValue<QnResourcePtr>(resource);
    case Qn::ResourceFlagsRole:
        return static_cast<int>(resource->flags());
    case Qn::ResourceSearchStringRole: 
        return resource->toSearchString();
    case Qn::ResourceStatusRole: 
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

