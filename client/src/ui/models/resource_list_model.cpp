#include "resource_list_model.h"

#include <client/client_globals.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>

#include <ui/common/ui_resource_name.h>
#include <ui/style/resource_icon_cache.h>

#include <utils/common/checked_cast.h>

QnResourceListModel::QnResourceListModel(QObject *parent) :
    base_type(parent),
    m_readOnly(false),
    m_checkable(false),
    m_statusIgnored(false),
    m_resources(),
    m_checkedResources()
{}

QnResourceListModel::~QnResourceListModel()
{
    return;
}

bool QnResourceListModel::isReadOnly() const
{
    return m_readOnly;
}

void QnResourceListModel::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
}

bool QnResourceListModel::isCheckable() const
{
    return m_checkable;
}

void QnResourceListModel::setCheckable(bool value)
{
    if (m_checkable == value)
        return;
    beginResetModel();
    m_checkable = value;
    endResetModel();
}

bool QnResourceListModel::isStatusIgnored() const
{
    return m_statusIgnored;
}

void QnResourceListModel::setStatusIgnored(bool value)
{
    if (m_statusIgnored == value)
        return;

    beginResetModel();
    m_statusIgnored = value;
    endResetModel();
}

const QnResourceList &QnResourceListModel::resources() const
{
    return m_resources;
}

void QnResourceListModel::setResources(const QnResourceList &resources)
{
    beginResetModel();

    foreach(const QnResourcePtr &resource, m_resources)
        disconnect(resource.data(), NULL, this, NULL);

    m_resources = resources;

    foreach(const QnResourcePtr &resource, m_resources) {
        connect(resource.data(), SIGNAL(nameChanged(const QnResourcePtr &)),    this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
        connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),  this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
        connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    }

    endResetModel();
}

void QnResourceListModel::addResource(const QnResourcePtr &resource)
{
    foreach(const QnResourcePtr &r, m_resources) // TODO: #Elric checking for duplicates does not belong here. Makes no sense!
        if (r->getId() == resource->getId())
            return;

    int row = m_resources.size();
    beginInsertRows(QModelIndex(), row, row);

    connect(resource.data(), SIGNAL(nameChanged(const QnResourcePtr &)),    this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),  this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),this, SLOT(at_resource_resourceChanged(const QnResourcePtr &)));
    m_resources << resource;

    endInsertRows();
}

void QnResourceListModel::removeResource(const QnResourcePtr &resource)
{
    if (!resource)
        return;

    int row;

    for (row = 0; row < m_resources.size(); ++row) {
        if (m_resources[row]->getId() == resource->getId()) // TODO: #Elric check by pointer, not id. Makes no sense.
            break;
    }

    if (row == m_resources.size())
        return;

    beginRemoveRows(QModelIndex(), row, row);

    m_resources[row]->disconnect(this);
    m_resources.removeAt(row);

    endRemoveRows();
}

QSet<QnUuid> QnResourceListModel::checkedResources() const
{
    return m_checkedResources;
}

void QnResourceListModel::setCheckedResources(const QSet<QnUuid>& ids)
{
    if (m_checkedResources == ids)
        return;

    beginResetModel();
    m_checkedResources = ids;
    endResetModel();
}

int QnResourceListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (m_checkable)
        return ColumnCount;
    return ColumnCount - 1; //CheckColumn is the last
}

int QnResourceListModel::rowCount(const QModelIndex &parent) const
{
    if(!parent.isValid())
        return m_resources.size();

    return 0;
}

Qt::ItemFlags QnResourceListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return Qt::NoItemFlags;

    int column = index.column();

    if (column == CheckColumn)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable;

    Qt::ItemFlags result = base_type::flags(index);
    if(m_readOnly)
        return result;

    const QnResourcePtr &resource = m_resources[index.row()];
    if(!resource)
        return result;

    return result | Qt::ItemIsEditable;
}


QVariant QnResourceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    int column = index.column();

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
        if (column == NameColumn)
            return getResourceName(resource);
        break;

    case Qt::EditRole:
        if (column == NameColumn)
            return getFullResourceName(resource, false);
        break;

    case Qt::DecorationRole:
        if (index.column() == NameColumn)
            return resourceIcon(resource);
        break;

    case Qt::CheckStateRole:
        if (index.column() == CheckColumn)
            return m_checkedResources.contains(resource->getId())
                ? Qt::Checked
                : Qt::Unchecked;
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

bool QnResourceListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return false;

    if (index.column() == CheckColumn && role == Qt::CheckStateRole)
    {
        const QnResourcePtr &resource = m_resources[index.row()];
        if (!resource)
            return false;

        bool checked = value.toInt() == Qt::Checked;
        if (checked)
            m_checkedResources.insert(resource->getId());
        else
            m_checkedResources.remove(resource->getId());
        emit dataChanged(index, index, QVector<int>() << Qt::CheckStateRole);
        return true;
    }
    return false;
}

QModelIndex QnResourceListModel::index(int row, int column, const QModelIndex &parent /*= QModelIndex()*/) const
{
    /* Here rowCount and columnCount are checked */
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex QnResourceListModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child)
    return QModelIndex();
}

void QnResourceListModel::at_resource_resourceChanged(const QnResourcePtr &resource)
{
    int row = m_resources.indexOf(resource);
    if(row == -1)
        return;

    QModelIndex index = this->index(row, 0);
    emit dataChanged(index, index);
}

QIcon QnResourceListModel::resourceIcon(const QnResourcePtr& resource) const
{
    if (!m_statusIgnored)
        return qnResIconCache->icon(resource);

    QnResourceIconCache::Key key = qnResIconCache->key(resource);
    key &= ~QnResourceIconCache::StatusMask;
    key |= QnResourceIconCache::Online;
    return qnResIconCache->icon(key);
}

