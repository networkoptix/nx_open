#include "time_server_selection_model.h"

#include <boost/algorithm/cxx11/any_of.hpp>
#include <algorithm>

#include <api/model/time_reply.h>
#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <text/time_strings.h>
#include <translation/datetime_formatter.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/synctime.h>

#include <nx_ec/ec_api.h>
#include <nx/client/core/watchers/server_time_watcher.h>
#include <nx/client/core/utils/human_readable.h>
#include <nx/utils/algorithm/index_of.h>
#include <nx/utils/guarded_callback.h>

namespace {

template<class T>
QVector<T> sortedVector(const QVector<T>& unsorted)
{
    auto sorted = unsorted;
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

QVector<unsigned int> kOffsetThresholdSeconds = sortedVector<unsigned int>(
    { 3, 5, 10, 30, 60, 180, 300, 600 });

QVector<int> kTextRoles = {
    Qt::DisplayRole,
    Qt::StatusTipRole,
    Qt::WhatsThisRole,
    Qt::AccessibleTextRole,
    Qt::AccessibleDescriptionRole,
    Qt::ToolTipRole };

QVector<int> kCheckboxRoles = {
    Qt::DisplayRole,
    Qt::CheckStateRole };

QVector<int> kForegroundRole = {
    Qt::ForegroundRole };

QString serverName(const QnResourcePtr& server)
{
    return server
        ? QnResourceDisplayInfo(server).toString(Qn::RI_NameOnly)
        : QnTimeServerSelectionModel::tr("Server");
}

} // namespace

QnTimeServerSelectionModel::QnTimeServerSelectionModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_sameTimezone(false),
    m_sameTimezoneValid(false),
    m_currentRequest(rest::Handle())
{
    auto processor = commonModule()->messageProcessor();

    updateTimeOffset();

    connect(processor, &QnCommonMessageProcessor::syncTimeChanged, this,
        [this](qint64 syncTime)
        {
            /* Requesting initial time. */
            resetData(syncTime);
        });

    /* Handle removing online server peers. */
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto uuid = resource->getId();
            int idx = nx::utils::algorithm::index_of(m_items,
                [uuid](const Item& item)
                {
                    return item.peerId == uuid;
                });

            if (idx < 0)
                return;

            NX_VERBOSE(this, lm("peer %1 is removed").arg(resource->getId()));
            {
                ScopedRemoveRows removeRows(this, QModelIndex(), idx, idx);
                m_items.removeAt(idx);
                updateFirstItemCheckbox();
            }

            if (m_selectedServer == uuid)
                setSelectedServer(QnUuid());
        });

    /* Handle adding new servers (to display name correctly). */
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            auto server = resourcePool()->getResourceById<QnMediaServerResource>(resource->getId());
            if (!server)
                return;

            NX_VERBOSE(this, lm("peer %1 is add").arg(resource->getId()));

            ScopedInsertRows insertRows(this, QModelIndex(), m_items.size(), m_items.size());
            addItem(server);
            updateFirstItemCheckbox();
            m_sameTimezoneValid = false;
        });

    const auto timeWatcher = context()->instance<nx::vms::client::core::ServerTimeWatcher>();
    connect(timeWatcher, &nx::vms::client::core::ServerTimeWatcher::displayOffsetsChanged, this,
        [this]
        {
            m_sameTimezoneValid = false;
            updateColumn(Columns::TimeColumn);
            updateColumn(Columns::OffsetColumn);
        });

    /* Requesting initial time. */
    resetData(qnSyncTime->currentMSecsSinceEpoch());
}

void QnTimeServerSelectionModel::updateTimeOffset()
{
    if (m_currentRequest)
        return;

    const auto server = commonModule()->currentServer();
    if (!server)
        return;
    auto apiConnection = server->restConnection();

    const auto callback = nx::utils::guarded(this,
        [this, apiConnection]
            (bool success, rest::Handle handle, rest::MultiServerTimeData data)
        {
            if (m_currentRequest != handle)
                return;

            m_currentRequest = rest::Handle(); //< Reset.
            if (!success)
                return;

            const auto syncTime = qnSyncTime->currentMSecsSinceEpoch();
            for (const auto& record: data.data)
            {
                // Store received value to use it later.
                const auto offset = record.osTime.count() - syncTime;
                m_serverOffsetCache[record.serverId] = offset;

                // Check if the server is already online.
                const auto idx = nx::utils::algorithm::index_of(m_items,
                    [&record](const Item& item)
                    {
                        return item.peerId == record.serverId;
                    });

                if (idx < 0)
                    break;

                m_items[idx].offset = offset;
                m_items[idx].ready = true;
                emit dataChanged(index(idx, TimeColumn), index(idx, OffsetColumn), kTextRoles);
            }
        });

    m_currentRequest = apiConnection->getTimeOfServersAsync(callback, thread());
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

    QnMediaServerResourcePtr server = resourcePool()->getResourceById<QnMediaServerResource>(item.peerId);
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
                        const auto timeWatcher =
                            context()->instance<nx::vms::client::core::ServerTimeWatcher>();

                        dateTime = timeWatcher->serverTime(server, sinceEpochMs);
                    }

                    auto offsetFromUtc = dateTime.offsetFromUtc();
                    dateTime.setTimeSpec(Qt::OffsetFromUTC);
                    dateTime.setOffsetFromUtc(offsetFromUtc);

                    switch (column)
                    {
                        case Columns::DateColumn:
                            return datetime::toString(dateTime.date());
                        case Columns::ZoneColumn:
                            return dateTime.timeZoneAbbreviation();
                        case Columns::TimeColumn:
                            return datetime::toString(dateTime.time());
                        default:
                            break;
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

        case Qt::ForegroundRole:
            if (column == Columns::OffsetColumn)
                return offsetForeground(item.offset);
            break;

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

void QnTimeServerSelectionModel::addItem(QnMediaServerResourcePtr server)
{
    Item item;
    item.peerId = server->getId();

    item.ready = m_serverOffsetCache.contains(item.peerId);
    if (item.ready)
    {
        item.offset = m_serverOffsetCache[item.peerId];
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

    NX_VERBOSE(this, lm("model: set selected peer to %1").arg(serverId));

    if (!serverId.isNull())
    {
        int idx = nx::utils::algorithm::index_of(m_items,
            [serverId](const Item& item){return item.peerId == serverId; });
        if (idx < 0)
        {
            NX_VERBOSE(this, lm("model: selected peer %1 is not found").arg(serverId));
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

    const auto& roles = column == Columns::CheckboxColumn
        ? kCheckboxRoles
        : kTextRoles;

    emit dataChanged(index(0, column), index(m_items.size() - 1, column), roles);
}

QString QnTimeServerSelectionModel::formattedOffset(qint64 offsetMs)
{
    const auto duration = std::chrono::milliseconds(offsetMs);
    static const QString kSeparator(L' ');

    using HumanReadable = nx::vms::client::core::HumanReadable;
    return HumanReadable::timeSpan(duration,
        HumanReadable::Hours | HumanReadable::Minutes | HumanReadable::Seconds,
        HumanReadable::SuffixFormat::Short,
        kSeparator,
        HumanReadable::kNoSuppressSecondUnit);
}

QVariant QnTimeServerSelectionModel::offsetForeground(qint64 offsetMs) const
{
    if (m_colors.empty())
        return QVariant();

    unsigned int offsetSeconds = static_cast<unsigned int>(qMin<qint64>(
        qAbs(offsetMs) / 1000, std::numeric_limits<unsigned int>::max()));

    int index = std::upper_bound(kOffsetThresholdSeconds.begin(), kOffsetThresholdSeconds.end(), offsetSeconds)
        - kOffsetThresholdSeconds.begin();

    return QVariant::fromValue(QBrush(m_colors[qBound(0, index, m_colors.size() - 1)]));
}

const QVector<QColor>& QnTimeServerSelectionModel::colors() const
{
    return m_colors;
}

void QnTimeServerSelectionModel::setColors(const QVector<QColor>& colors)
{
    if (colors == m_colors)
        return;

    m_colors = colors;

    if (!m_items.isEmpty())
        emit dataChanged(index(0, OffsetColumn), index(m_items.size() - 1, OffsetColumn), kForegroundRole);
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
    qint64 commonUtcOffset = Qn::InvalidUtcOffset;

    for (const auto& item: m_items)
    {
        const auto server = resourcePool()->getResourceById<QnMediaServerResource>(item.peerId);
        if (!server || server->getStatus() != Qn::Online)
            continue;

        const auto utcOffset = server->utcOffset();
        if (utcOffset == Qn::InvalidUtcOffset)
            continue;

        if (commonUtcOffset == Qn::InvalidUtcOffset)
            commonUtcOffset = utcOffset;
        else if (commonUtcOffset != utcOffset)
            return false;
    }

    return true;
}

void QnTimeServerSelectionModel::resetData(qint64 currentSyncTime)
{
    Q_UNUSED(currentSyncTime);
    updateTimeOffset();

    /* Fill table with current data. */
    ScopedReset modelReset(this);
    m_items.clear();
    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
        addItem(server);
}
