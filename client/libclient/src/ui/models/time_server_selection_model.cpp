#include "time_server_selection_model.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_runtime_data.h>

#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <nx/utils/collection.h>
#include <utils/common/qtimespan.h>
#include <utils/common/synctime.h>
#include <utils/tz/tz.h>

namespace {
    QVector<int> textRoles = QVector<int>()
        << Qt::DisplayRole
        << Qt::StatusTipRole
        << Qt::WhatsThisRole
        << Qt::AccessibleTextRole
        << Qt::AccessibleDescriptionRole
        << Qt::ToolTipRole;

    QVector<int> checkboxRoles = QVector<int>()
        << Qt::DisplayRole
        << Qt::CheckStateRole;

    QString serverName(const QnResourcePtr& server)
    {
        return server
            ? QnResourceDisplayInfo(server).toString(Qn::RI_NameOnly)
            : QnTimeServerSelectionModel::tr("Server");
    }

} // namespace

#ifdef _DEBUG
    //#define QN_TIME_SERVER_SELECTION_MODEL_DEBUG
#endif

#ifdef QN_TIME_SERVER_SELECTION_MODEL_DEBUG
#define PRINT_DEBUG(MSG) qDebug() << MSG
#else
#define PRINT_DEBUG(MSG)
#endif

QnTimeServerSelectionModel::QnTimeServerSelectionModel(QObject* parent /* = NULL*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_sameTimezone(false),
    m_sameTimezoneValid(false)
{
    auto processor = QnCommonMessageProcessor::instance();

    /* Handle peer time updates. */
    connect(processor, &QnCommonMessageProcessor::peerTimeChanged, this,
        [this](const QnUuid& peerId, qint64 syncTime, qint64 peerTime)
        {
            /* Store received value to use it later. */
            qint64 offset = peerTime - syncTime;
            m_serverOffsetCache[peerId] = offset;
            PRINT_DEBUG("get time for peer " + peerId.toByteArray());

            /* Check if the server is already online. */
            int idx = qnIndexOf(m_items,
                [peerId](const Item& item)
                {
                    return item.peerId == peerId;
                });

            if (idx < 0)
                return;

            m_items[idx].offset = offset;
            m_items[idx].ready = true;
            PRINT_DEBUG("peer " + peerId.toByteArray() + " is ready");
            emit dataChanged(index(idx, TimeColumn), index(idx, OffsetColumn), textRoles);
        });

    connect(processor, &QnCommonMessageProcessor::syncTimeChanged, this,
        [this](qint64 syncTime)
        {
            /* Requesting initial time. */
            resetData(syncTime);
        });

    /* Handle adding new online server peers. */
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this,
        [this](const QnPeerRuntimeInfo& info)
        {
            if (info.data.peer.peerType != Qn::PT_Server)
                return;

            ScopedInsertRows insertRows(this, QModelIndex(), m_items.size(), m_items.size());
            addItem(info);
            updateFirstItemCheckbox();
        });

    /* Handle changing server peers priority (selection). */
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this,
        [this](const QnPeerRuntimeInfo& info)
        {
            if (info.data.peer.peerType != Qn::PT_Server)
                return;

            PRINT_DEBUG("peer " + info.uuid.toByteArray() + " is changed");
            if (isSelected(info.data.serverTimePriority))
                setSelectedServer(info.uuid);
            else if (info.uuid == m_selectedServer)
                setSelectedServer(QnUuid());
        });

    /* Handle removing online server peers. */
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoRemoved, this,
        [this](const QnPeerRuntimeInfo& info)
        {
            if (info.data.peer.peerType != Qn::PT_Server)
                return;

            int idx = qnIndexOf(m_items,
                [info](const Item& item)
                {
                    return item.peerId == info.uuid;
                });

            if (idx < 0)
                return;

            PRINT_DEBUG("peer " + info.uuid.toByteArray() + " is removed");
            {
                ScopedRemoveRows removeRows(this, QModelIndex(), idx, idx);
                m_items.removeAt(idx);
                updateFirstItemCheckbox();
            }

            if (m_selectedServer == info.uuid)
                setSelectedServer(QnUuid());
        });

    /* Handle adding new servers (to display name correctly). */
    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (!resource.dynamicCast<QnMediaServerResource>())
                return;

            QnUuid id = resource->getId();
            int idx = qnIndexOf(m_items,
                [id](const Item& item)
                {
                    return item.peerId == id;
                });

            if (idx < 0)
                return;

            m_sameTimezoneValid = false;
            updateColumn(Columns::TimeColumn);
            updateColumn(Columns::OffsetColumn);

            emit dataChanged(
                this->index(idx, Columns::NameColumn),
                this->index(idx, Columns::NameColumn),
                textRoles);
        });

    connect(context()->instance<QnWorkbenchServerTimeWatcher>(), &QnWorkbenchServerTimeWatcher::displayOffsetsChanged, this,
        [this]
        {
            m_sameTimezoneValid = false;
            updateColumn(Columns::TimeColumn);
            updateColumn(Columns::OffsetColumn);
        });

    /* Requesting initial time. */
    resetData(qnSyncTime->currentMSecsSinceEpoch());
}

void QnTimeServerSelectionModel::updateFirstItemCheckbox()
{
    if (m_items.empty())
        return;

    const auto firstServerIndex = index(0, CheckboxColumn);
    dataChanged(firstServerIndex, firstServerIndex);  // Updates enable state of first server checkbox
}

int QnTimeServerSelectionModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return Columns::ColumnCount;
}

int QnTimeServerSelectionModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
        return 0;

    return m_items.size();
}

QVariant QnTimeServerSelectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
            case Columns::CheckboxColumn:
                return QString();
            case Columns::NameColumn:
                return tr("Server");
            case Columns::DateColumn:
                return tr("Date");
            case Columns::ZoneColumn:
                return tr("Timezone");
            case Columns::TimeColumn:
                return tr("Time");
            case Columns::OffsetColumn:
                return tr("Offset");
            default:
                break;
        }
    }
    return QVariant();
}

QVariant QnTimeServerSelectionModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    if (index.row() < 0 || index.row() >= m_items.size())
        return QVariant();

    Columns column = static_cast<Columns>(index.column());
    Item item = m_items[index.row()];

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(item.peerId);
    QString title = serverName(server);

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::StatusTipRole:
        case Qt::AccessibleTextRole:
        case Qt::ToolTipRole:
        {
            switch (column)
            {
                case Columns::NameColumn:
                    return title;

                case Columns::DateColumn:
                case Columns::ZoneColumn:
                case Columns::TimeColumn:
                {
                    if (!item.ready)
                        return QString();

                    QDateTime dateTime;
                    auto sinceEpochMs = currentTime + item.offset;

                    if (sameTimezone())
                    {
                        dateTime.setTimeSpec(Qt::LocalTime);
                        dateTime.setMSecsSinceEpoch(sinceEpochMs);
                    }
                    else
                    {
                        dateTime = context()->instance<QnWorkbenchServerTimeWatcher>()->
                            serverTime(server, sinceEpochMs);
                    }

                    auto offsetFromUtc = dateTime.offsetFromUtc();
                    dateTime.setTimeSpec(Qt::OffsetFromUTC);
                    dateTime.setOffsetFromUtc(offsetFromUtc);

                    switch (column)
                    {
                        case Columns::DateColumn:
                            return dateTime.toString(lit("dd/MM/yyyy"));
                        case Columns::ZoneColumn:
                            return dateTime.timeZoneAbbreviation();
                        case Columns::TimeColumn:
                            return dateTime.toString(lit("HH:mm:ss"));
                    }
                }

                case Columns::OffsetColumn:
                {
                    return item.ready
                        ? formattedOffset(item.offset)
                        : QString();
                }

                default:
                    break;
            }

            break;
        }

        case Qt::DecorationRole:
            if (column == Columns::NameColumn)
            {
                if (QnMediaServerResource::isHiddenServer(server))
                    return qnResIconCache->icon(QnResourceIconCache::Camera);
                return qnResIconCache->icon(QnResourceIconCache::Server);
            }
            break;

        case Qt::CheckStateRole:
            if (column == Columns::CheckboxColumn)
            {
                return item.peerId == m_selectedServer || m_items.size() == 1
                    ? Qt::Checked
                    : Qt::Unchecked;
            }
            break;

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(server);

        case Qn::PriorityRole:
            return item.priority;

        default:
            break;
    }

    return QVariant();
}

bool QnTimeServerSelectionModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return 0;

    if (index.row() < 0 || index.row() >= m_items.size())
        return 0;

    if (index.column() != Columns::CheckboxColumn || role != Qt::CheckStateRole)
        return false;

    Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());

    // do not allow to uncheck element
    if (state == Qt::Unchecked)
        return false;

    setSelectedServer(m_items[index.row()].peerId);
    return true;
}

Qt::ItemFlags QnTimeServerSelectionModel::flags(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()))
        return 0;

    if (index.row() < 0 || index.row() >= m_items.size())
        return 0;

    Columns column = static_cast<Columns>(index.column());
    Item item = m_items[index.row()];

    if (column == Columns::CheckboxColumn)
    {
        static const Qt::ItemFlags kBaseCheckboxFlags =
            (Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);

        const Qt::ItemFlag anotherFlags = Qt::ItemIsEnabled;// (m_items.size() == 1 ? Qt::NoItemFlags : Qt::ItemIsEnabled);
        return kBaseCheckboxFlags | anotherFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void QnTimeServerSelectionModel::updateTime()
{
    updateColumn(Columns::TimeColumn);
}

void QnTimeServerSelectionModel::addItem(const QnPeerRuntimeInfo& info)
{
    PRINT_DEBUG("peer " + info.uuid.toByteArray() + " is added");
#ifdef _DEBUG
    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(info.uuid);
    QString title = serverName(server);
    PRINT_DEBUG("peer " + info.uuid.toByteArray() + " name is " + title.toUtf8());
#endif // DEBUG

    Item item;
    item.peerId = info.uuid;
    item.priority = info.data.serverTimePriority;
    if (isSelected(item.priority))
    {
        PRINT_DEBUG("peer " + info.uuid.toByteArray() + " is SELECTED");
        m_selectedServer = item.peerId;
    }

    item.ready = m_serverOffsetCache.contains(item.peerId);
    if (item.ready)
    {
        item.offset = m_serverOffsetCache[item.peerId];
        PRINT_DEBUG("peer " + info.uuid.toByteArray() + " is ready");
    }
    m_items << item;

    m_sameTimezoneValid = false;
}

QnUuid QnTimeServerSelectionModel::selectedServer() const
{
    return m_selectedServer;
}

void QnTimeServerSelectionModel::setSelectedServer(const QnUuid& serverId)
{
    if (m_selectedServer == serverId)
        return;

    PRINT_DEBUG("model: set selected peer to " + serverId.toByteArray());

    if (!serverId.isNull())
    {
        int idx = qnIndexOf(m_items, [serverId](const Item& item){return item.peerId == serverId; });
        if (idx < 0)
        {
            PRINT_DEBUG("model: selected peer " + serverId.toByteArray() + " NOT FOUND");
            return;
        }
    }

    m_selectedServer = serverId;
    updateColumn(Columns::CheckboxColumn);
}

void QnTimeServerSelectionModel::updateColumn(Columns column)
{
    if (m_items.isEmpty())
        return;

    QVector<int> roles = column == Columns::CheckboxColumn
        ? checkboxRoles
        : textRoles;

    emit dataChanged(index(0, column), index(m_items.size() - 1, column), roles);
}

bool QnTimeServerSelectionModel::isSelected(quint64 priority)
{
    return (priority & ec2::ApiRuntimeData::tpfPeerTimeSetByUser) > 0;
}

QString QnTimeServerSelectionModel::formattedOffset(qint64 offsetMSec)
{
    static const Qt::TimeSpanFormat kFormat = Qt::Seconds | Qt::Minutes | Qt::Hours;
    static const int kDoNotSuppress = -1;
    static const int kMinimalOffsetMs = 1000;

    offsetMSec = qAbs(offsetMSec);

    if (offsetMSec < kMinimalOffsetMs)
        return QString();

    return QTimeSpan(offsetMSec).toApproximateString(kDoNotSuppress, kFormat);
}

bool QnTimeServerSelectionModel::sameTimezone() const
{
    if (m_sameTimezoneValid)
        return m_sameTimezone;

    m_sameTimezone = calculateSameTimezone();
    m_sameTimezoneValid = true;
    return m_sameTimezone;
}

bool QnTimeServerSelectionModel::calculateSameTimezone() const
{
    auto watcher = context()->instance<QnWorkbenchServerTimeWatcher>();
    qint64 localOffset = nx_tz::getLocalTimeZoneOffset(); /* In minutes. */
    qint64 commonOffset = localOffset == -1
        ? Qn::InvalidUtcOffset
        : localOffset*60;

    for (const auto& item : m_items)
    {
        QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(item.peerId);
        if (!server || server->getStatus() != Qn::Online)
            continue;

        auto offsetMs = watcher->utcOffset(server);
        if (offsetMs == Qn::InvalidUtcOffset)
            continue;

        auto offset = offsetMs / 1000;

        if (commonOffset == Qn::InvalidUtcOffset)
            commonOffset = offset;
        else if (commonOffset != offset)
            return false;
    }

    return true;
}

void QnTimeServerSelectionModel::resetData(qint64 currentSyncTime)
{
    if (auto connection = QnAppServerConnectionFactory::getConnection2())
    {
        auto timeManager = connection->getTimeManager(Qn::kSystemAccess);
        for (const auto& info : timeManager->getPeerTimeInfoList())
            m_serverOffsetCache[info.peerId] = info.time - currentSyncTime;
    }

    /* Fill table with current data. */
    ScopedReset modelReset(this);
    m_items.clear();
    for (const auto& runtimeInfo : qnRuntimeInfoManager->items()->getItems())
    {
        if (runtimeInfo.data.peer.peerType != Qn::PT_Server)
            continue;
        addItem(runtimeInfo);
    }
}
