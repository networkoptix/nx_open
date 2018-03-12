#include "resource_list_model.h"

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>

#include <ui/style/resource_icon_cache.h>

#include <utils/common/checked_cast.h>

QnResourceListModel::QnResourceListModel(QObject *parent) :
    base_type(parent)
{
}

QnResourceListModel::~QnResourceListModel()
{
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

    ScopedReset resetModel(this);
    m_hasCheckboxes = value;
}

bool QnResourceListModel::userCheckable() const
{
    return m_userCheckable;
}

void QnResourceListModel::setUserCheckable(bool value)
{
    m_userCheckable = value;
}

void QnResourceListModel::setSinglePick(bool value)
{
    m_singlePick = value;
}

bool QnResourceListModel::hasStatus() const
{
    return m_hasStatus;
}

void QnResourceListModel::setHasStatus(bool value)
{
    if (m_hasStatus == value)
        return;
    ScopedReset resetModel(this);
    m_hasStatus = value;
}

QnResourceListModel::Options QnResourceListModel::options() const
{
    return m_options;
}

void QnResourceListModel::setOptions(Options value)
{
    if (m_options == value)
        return;

    ScopedReset resetModel(this);
    m_options = value;
}

const QnResourceList &QnResourceListModel::resources() const
{
    return m_resources;
}

void QnResourceListModel::setResources(const QnResourceList &resources)
{
    ScopedReset resetModel(this);

    foreach(const QnResourcePtr &resource, m_resources)
        disconnect(resource, nullptr, this, nullptr);

    m_resources = resources;

    for (const QnResourcePtr &resource : m_resources)
    {
        connect(resource, &QnResource::nameChanged,    this, &QnResourceListModel::at_resource_resourceChanged);
        connect(resource, &QnResource::statusChanged,  this, &QnResourceListModel::at_resource_resourceChanged);
        connect(resource, &QnResource::resourceChanged,this, &QnResourceListModel::at_resource_resourceChanged);
    }
}

void QnResourceListModel::addResource(const QnResourcePtr &resource)
{
    NX_ASSERT(m_resources.indexOf(resource) < 0);

    connect(resource, &QnResource::nameChanged, this, &QnResourceListModel::at_resource_resourceChanged);
    connect(resource, &QnResource::statusChanged, this, &QnResourceListModel::at_resource_resourceChanged);
    connect(resource, &QnResource::resourceChanged, this, &QnResourceListModel::at_resource_resourceChanged);

    int row = m_resources.size();
    ScopedInsertRows insertRows(this, QModelIndex(), row, row);
    m_resources << resource;
}

void QnResourceListModel::removeResource(const QnResourcePtr &resource)
{
    if (!resource)
        return;

    int row = m_resources.indexOf(resource);
    if (row < 0)
        return;

    ScopedRemoveRows removeRows(this, QModelIndex(), row, row);

    m_resources[row]->disconnect(this);
    m_resources.removeAt(row);
}

QSet<QnUuid> QnResourceListModel::checkedResources() const
{
    return m_checkedResources;
}

void QnResourceListModel::setCheckedResources(const QSet<QnUuid>& ids)
{
    if (m_checkedResources == ids)
        return;

    ScopedReset resetModel(this);

    m_checkedResources.clear();

    // Need to filter out IDs that are not in this resource list.
    // We will gather all resource ids to a separate set, and then check ids 
    // from selection with this set. O(NlogM) is here.
    QSet<QnUuid> contained_ids;
    for (const auto& ptr: m_resources)
    {
        auto id = ptr->getId();
        contained_ids.insert(id);
    }

    for (const auto& id: ids)
    {
        if (contained_ids.contains(id))
            m_checkedResources.insert(id);
    }
}

int QnResourceListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    int result = 1;     //< Added name column by default.
    result += m_hasCheckboxes;
    result += m_hasStatus;
    return result;
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

    if (m_customAccessors.contains(column))
        return m_customAccessors[column](resource, role);

    switch(role)
    {
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
            if (column == NameColumn)
                return resourceIcon(resource);
            break;

        case Qt::CheckStateRole:
            if (column == CheckColumn)
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
        case Qn::NodeTypeRole:
            return m_options.testFlag(ServerAsHealthMonitorOption)
                ? qVariantFromValue(Qn::LayoutItemNode)
                : qVariantFromValue(Qn::ResourceNode);

        default:
            break;
    }

    return QVariant();
}

void QnResourceListModel::setCustomColumnAccessor(int column, ColumnDataAccessor dataAccessor)
{
    m_customAccessors[column] = dataAccessor;
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
        bool changed = false;

        qDebug() << " QnResourceListModel::setData(" << resource->getName() << ") check changed to " << checked;
        auto id = resource->getId();

        // Making a conservative changes. Do not raise event if nothing has changed
        if(m_singlePick)
        {
            if(checked)
            {
                if (!m_checkedResources.contains(id))
                {
                    ScopedReset resetModel(this);
                    m_checkedResources = QSet<QnUuid>{ id };
                    // TODO: Notify all other elements about the changes
                    changed = true;
                }
            }
            else
            {
                if (m_checkedResources.contains(id))
                {
                    ScopedReset resetModel(this);
                    // TODO: Notify all other elements about the changes
                    m_checkedResources = QSet<QnUuid>();
                    changed = true;
                }
            }
        }
        else
        {
            if (checked)
                m_checkedResources.insert(id);
            else
                m_checkedResources.remove(id);
            changed = true;
        }

        if(changed)
        {
            emit selectionChanged();
        }

        emit dataChanged(index.sibling(index.row(), 0),
            index.sibling(index.row(), ColumnCount - 1),
            { Qt::CheckStateRole });
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
    QnResourceIconCache::Key addionalKey = 0;
    if (m_options.testFlag(AlwaysSelectedOption))
        addionalKey = QnResourceIconCache::AlwaysSelected;

    if (resource->hasFlags(Qn::server) && m_options.testFlag(ServerAsHealthMonitorOption))
        return qnResIconCache->icon(QnResourceIconCache::HealthMonitor | addionalKey);

    if (m_options.testFlag(HideStatusOption))
    {
        QnResourceIconCache::Key key = qnResIconCache->key(resource);
        key &= ~QnResourceIconCache::StatusMask;
        key |= QnResourceIconCache::Online;
        return qnResIconCache->icon(key | addionalKey);
    }

    return qnResIconCache->icon(qnResIconCache->key(resource) | addionalKey);
}

