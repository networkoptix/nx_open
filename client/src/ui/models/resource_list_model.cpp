#include "resource_list_model.h"

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>

#include <ui/style/resource_icon_cache.h>

#include <utils/common/checked_cast.h>

QnResourceListModel::QnResourceListModel(QObject *parent) :
    base_type(parent),
    m_readOnly(false),
    m_hasCheckboxes(false),
    m_userCheckable(true),
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

bool QnResourceListModel::hasCheckboxes() const
{
    return m_hasCheckboxes;
}

void QnResourceListModel::setHasCheckboxes(bool value)
{
    if (m_hasCheckboxes == value)
        return;
    beginResetModel();
    m_hasCheckboxes = value;
    endResetModel();
}

bool QnResourceListModel::userCheckable() const
{
    return m_userCheckable;
}

void QnResourceListModel::setUserCheckable(bool value)
{
    m_userCheckable = value;
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
        disconnect(resource, nullptr, this, nullptr);

    m_resources = resources;

    for (const QnResourcePtr &resource : m_resources)
    {
        connect(resource, &QnResource::nameChanged,    this, &QnResourceListModel::at_resource_resourceChanged);
        connect(resource, &QnResource::statusChanged,  this, &QnResourceListModel::at_resource_resourceChanged);
        connect(resource, &QnResource::resourceChanged,this, &QnResourceListModel::at_resource_resourceChanged);
    }

    endResetModel();
}

void QnResourceListModel::addResource(const QnResourcePtr &resource)
{
    NX_ASSERT(m_resources.indexOf(resource) < 0);

    int row = m_resources.size();
    beginInsertRows(QModelIndex(), row, row);

    connect(resource, &QnResource::nameChanged,     this, &QnResourceListModel::at_resource_resourceChanged);
    connect(resource, &QnResource::statusChanged,   this, &QnResourceListModel::at_resource_resourceChanged);
    connect(resource, &QnResource::resourceChanged, this, &QnResourceListModel::at_resource_resourceChanged);
    m_resources << resource;

    endInsertRows();
}

void QnResourceListModel::removeResource(const QnResourcePtr &resource)
{
    if (!resource)
        return;

    int row = m_resources.indexOf(resource);
    if (row < 0)
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
    if (m_hasCheckboxes)
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
    Qt::ItemFlag userCheckableFlag = m_userCheckable ? Qt::ItemIsUserCheckable : Qt::NoItemFlags;

    if (column == CheckColumn)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | userCheckableFlag;

    Qt::ItemFlags result = base_type::flags(index);
    if(m_readOnly)
        return result;

    const QnResourcePtr &resource = m_resources[index.row()];
    if (!resource)
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
            return QnResourceDisplayInfo(resource).toString(Qn::RI_NameOnly);
        break;

    case Qt::EditRole:
        if (column == NameColumn)
            return QnResourceDisplayInfo(resource).toString(Qn::RI_NameOnly);
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

    //TODO: #vkutin #GDM #common Refactor/replace this role
    case Qn::DisabledRole:
        return m_checkedResources.contains(resource->getId())
            ? false
            : true;

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

        emit dataChanged(index.sibling(index.row(), 0),
                         index.sibling(index.row(), ColumnCount - 1),
                         { Qt::CheckStateRole, Qn::DisabledRole });
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

