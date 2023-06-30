// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_updates_model.h"

#include <QtWidgets/QApplication>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/utils/string.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/watchers/workbench_version_mismatch_watcher.h>
#include <ui/workbench/workbench_context.h>

#include "peer_state_tracker.h"

namespace nx::vms::client::desktop {

ServerUpdatesModel::ServerUpdatesModel(
    std::shared_ptr<PeerStateTracker> tracker,
    QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    m_tracker = tracker;
    // Dat strange thingy.
    connect(context()->instance<QnWorkbenchVersionMismatchWatcher>(),
        &QnWorkbenchVersionMismatchWatcher::mismatchDataChanged, this,
        [this]()
        {
            forceUpdateColumn(VersionColumn);
        });

    connect(m_tracker.get(), &PeerStateTracker::itemAdded,
        this, &ServerUpdatesModel::atItemAdded);
    connect(m_tracker.get(), &PeerStateTracker::itemToBeRemoved,
        this, &ServerUpdatesModel::atItemRemoved);
    connect(m_tracker.get(), &PeerStateTracker::itemChanged,
        this, &ServerUpdatesModel::atItemChanged);
}

int ServerUpdatesModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

int ServerUpdatesModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;
    return m_tracker->peersCount();
}

QVariant ServerUpdatesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case NameColumn:
                return tr("Component");
            case VersionColumn:
                return tr("Installed Version");
            case ProgressColumn:
                return tr("Status");
            case StatusMessageColumn:
                return tr("Message");
            case StorageSettingsColumn:
                return tr("Store Update Files");
            default:
                break;
        }
    }
    return QVariant();
}

QVariant ServerUpdatesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    auto item = m_tracker->findItemByRow(index.row());

    NX_ASSERT(item);
    if (!item)
        return QVariant();

    auto server = m_tracker->getServer(item);

    int column = index.column();

    switch (role)
    {
        case Qt::ForegroundRole:
            if (column == VersionColumn)
            {
                if (item->offline)
                    return core::colorTheme()->color("dark11");

                if (m_targetVersion.isNull() ||  m_targetVersion <= item->version)
                    return core::colorTheme()->color("green_l2");

                return core::colorTheme()->color("yellow_core");
            }
            else if (column == NameColumn && item->offline)
            {
                return core::colorTheme()->color("dark11");
            }
            break;

        case ServerUpdatesModel::UpdateItemRole:
            return QVariant::fromValue(item);

        case Qt::CheckStateRole:
            if (column == StorageSettingsColumn)
                return item->storeUpdates ? Qt::Checked : Qt::Unchecked;
            break;
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            switch (column)
            {
                case NameColumn:
                    if (item->component == UpdateItem::Component::server)
                        return QnResourceDisplayInfo(server).toString(
                            Qn::ResourceInfoLevel::RI_NameOnly);
                    else
                        return QString(tr("Client"));
                case VersionColumn:
                    if (item->offline)
                        return nx::UnicodeChars::kEnDash;
                    return item->version.toString();
                case StatusMessageColumn:
                    return item->statusMessage;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (item->component == UpdateItem::Component::server)
    {
        switch (role)
        {
            case core::ResourceRole:
                return QVariant::fromValue<QnResourcePtr>(server);

            case Qt::DecorationRole:
                if (column == NameColumn)
                    return qnResIconCache->icon(server);
                break;
        }
    }
    else
    {
        switch (role)
        {
            case Qt::DecorationRole:
                if (column == NameColumn)
                    return qnResIconCache->icon(QnResourceIconCache::Client);
                break;
            default:
                break;
        }
    }
    return QVariant();
}

Qt::ItemFlags ServerUpdatesModel::flags(const QModelIndex& index) const
{
    if (index.column() == ProgressColumn)
        return {Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled};
    if (index.column() == StorageSettingsColumn)
        return {Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled};
    return base_type::flags(index);
}

void ServerUpdatesModel::forceUpdateColumn(Columns column)
{
    int count = m_tracker->peersCount();
    if (!count)
        return;
    NX_ASSERT(column < Columns::ColumnCount);
    emit dataChanged(index(0, column), index(count - 1, column));
}

void ServerUpdatesModel::atItemAdded(UpdateItemPtr item)
{
    NX_ASSERT(item);
    if (!item)
        return;

    int row = item->row;
    beginInsertRows(QModelIndex(), row, row);
    atItemChanged(item);
    endInsertRows();
}

void ServerUpdatesModel::atItemRemoved(UpdateItemPtr item)
{
    NX_ASSERT(item);
    if (!item)
        return;

    QModelIndex idx = createIndex(item->row, 0);
    if (!idx.isValid())
        return;

    beginRemoveRows(QModelIndex(), idx.row(), idx.row());
    endRemoveRows();
}

void ServerUpdatesModel::atItemChanged(UpdateItemPtr item)
{
    if (!item)
        return;
    QModelIndex idx = createIndex(item->row, 0);
    emit dataChanged(idx, idx.sibling(idx.row(), ColumnCount - 1));
}

void ServerUpdatesModel::setUpdateTarget(const nx::utils::SoftwareVersion& version)
{
    m_targetVersion = version;
    forceUpdateColumn(VersionColumn);
}

SortedPeerUpdatesModel::SortedPeerUpdatesModel(QObject* parent):
    QSortFilterProxyModel(parent)
{
}

bool SortedPeerUpdatesModel::lessThan(const QModelIndex& leftIndex, const QModelIndex& rightIndex) const
{
    auto left = leftIndex.data(ServerUpdatesModel::UpdateItemRole).value<UpdateItemPtr>();
    auto right = rightIndex.data(ServerUpdatesModel::UpdateItemRole).value<UpdateItemPtr>();

    if (!left || !right)
        return left != nullptr;

    if (left->component != right->component)
        return left->component == UpdateItem::Component::client;

    if (left->incompatible != right->incompatible)
        return !left->incompatible;

    QString lname = leftIndex.data(Qt::DisplayRole).toString();
    QString rname = rightIndex.data(Qt::DisplayRole).toString();

    int result = nx::utils::naturalStringCompare(lname, rname, Qt::CaseInsensitive);
    if (result != 0)
        return result < 0;

    // We want the order to be defined even for items with the same name.
    return left->id < right->id;
}

} // namespace nx::vms::client::desktop
