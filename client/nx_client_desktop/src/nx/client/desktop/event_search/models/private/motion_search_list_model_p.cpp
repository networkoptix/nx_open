#include "motion_search_list_model_p.h"

#include <QtCore/QTimer>
#include <QtWidgets/QMenu>

#include <api/helpers/chunks_request_data.h>
#include <api/media_server_connection.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <motion/motion_detection.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/utils/managed_camera_set.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/algorithm/merge_sorted_lists.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/event/event_fwd.h>

#include <ini.h>

namespace nx::client::desktop {

namespace {

using namespace std::chrono;

static constexpr auto kLiveUpdateInterval = 15s;

static constexpr qreal kPreviewTimeFraction = 0.5;

microseconds midTime(const QnTimePeriod& period, qreal fraction = kPreviewTimeFraction)
{
    return period.isInfinite()
        ? microseconds(DATETIME_NOW)
        : microseconds(milliseconds(qint64(period.startTimeMs + period.durationMs * fraction)));
}

static const auto lowerBoundPredicate =
    [](const MotionChunk& left, milliseconds right) { return left.period.startTime() > right; };

static const auto upperBoundPredicate =
    [](milliseconds left, const MotionChunk& right) { return left > right.period.startTime(); };

} // namespace

MotionSearchListModel::Private::Private(MotionSearchListModel* q):
    base_type(q),
    q(q),
    m_liveUpdateTimer(new QTimer())
{
    connect(m_liveUpdateTimer.data(), &QTimer::timeout, this, &Private::fetchLive);
    m_liveUpdateTimer->start(kLiveUpdateInterval);
}

MotionSearchListModel::Private::~Private()
{
}

int MotionSearchListModel::Private::count() const
{
    return int(m_data.size());
}

QVariant MotionSearchListModel::Private::data(const QModelIndex& index, int role,
    bool& handled) const
{
    const auto& chunk = m_data[index.row()];
    handled = true;

    using namespace std::chrono;

    static const auto kDefaultLocale = QString();
    switch (role)
    {
        case Qt::DisplayRole:
            return tr("Motion");

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap("tree/camera.svg"));

        case Qn::HelpTopicIdRole:
            return QnBusiness::eventHelpId(vms::api::EventType::cameraMotionEvent);

        case Qn::TimestampRole:
            return QVariant::fromValue(microseconds(chunk.period.startTime()).count());

        case Qn::DurationRole:
            return QVariant::fromValue(microseconds(chunk.period.duration()).count());

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera(chunk));

        case Qn::ResourceListRole:
        {
            const auto resource = camera(chunk);
            return resource
                ? QVariant::fromValue(QnResourceList({resource}))
                : QVariant::fromValue(QStringList({QString("<%1>").arg(tr("deleted camera"))}));
        }

        case Qn::DescriptionTextRole:
        {
            if (!ini().showDebugTimeInformationInRibbon)
                return QString();

            return QString("Begin: %1<br>End: %2")
                .arg(utils::timestampToDebugString(chunk.period.startTimeMs))
                .arg(utils::timestampToDebugString(chunk.period.endTimeMs()));
        }

        case Qn::PreviewTimeRole:
            return QVariant::fromValue(midTime(chunk.period).count());

        case Qn::ForcePrecisePreviewRole:
            return true;

        case Qn::ContextMenuRole:
            return QVariant::fromValue(contextMenu(chunk));

        default:
            handled = false;
            return QVariant();
    }
}

QList<QRegion> MotionSearchListModel::Private::filterRegions() const
{
    return m_filterRegions;
}

void MotionSearchListModel::Private::setFilterRegions(const QList<QRegion>& value)
{
    if (m_filterRegions == value)
        return;

    q->clear();
    m_filterRegions = value;
}

bool MotionSearchListModel::Private::isCameraApplicable(
    const QnVirtualCameraResourcePtr& camera) const
{
    NX_ASSERT(camera);
    return camera && camera->hasFlags(Qn::motion);
}

bool MotionSearchListModel::Private::hasAccessRights() const
{
    return true;
}

void MotionSearchListModel::Private::clearData()
{
    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_prefetch.clear();
    m_liveFetch = {};
}

void MotionSearchListModel::Private::truncateToMaximumCount()
{
    this->truncateDataToMaximumCount(m_data,
        [](const MotionChunk& chunk) { return chunk.period.startTime(); });
}

void MotionSearchListModel::Private::truncateToRelevantTimePeriod()
{
    this->truncateDataToTimePeriod(
        m_data, upperBoundPredicate, q->relevantTimePeriod());
}

rest::Handle MotionSearchListModel::Private::requestPrefetch(const QnTimePeriod& period)
{
    const auto sortOrder = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    return getMotion(period, sortOrder, currentRequest().batchSize);
}

template<typename Iter>
bool MotionSearchListModel::Private::commitInternal(const QnTimePeriod& periodToCommit,
    Iter prefetchBegin, Iter prefetchEnd, int position, bool handleOverlaps)
{
    const auto begin = std::lower_bound(prefetchBegin, prefetchEnd,
        periodToCommit.endTime(), lowerBoundPredicate);

    auto end = std::upper_bound(prefetchBegin, prefetchEnd,
        periodToCommit.startTime(), upperBoundPredicate);

    if (handleOverlaps && !m_data.empty())
    {
        const auto& last = m_data.front();
        const auto lastTime = last.period.startTime();

        while (end != begin)
        {
            const auto iter = end - 1;
            if (iter->period.startTime() > lastTime)
                break;

            end = iter;

            if (*iter == last)
                break;
        }
    }

    const auto count = std::distance(begin, end);
    if (count <= 0)
    {
        NX_VERBOSE(q) << "Committing no motion periods";
        return false;
    }

    NX_VERBOSE(q) << "Committing" << count << "motion periods from"
        << utils::timestampToDebugString((end - 1)->period.startTimeMs) << "to"
        << utils::timestampToDebugString(begin->period.startTimeMs);

    ScopedInsertRows insertRows(q, position, position + count - 1);

    m_data.insert(m_data.begin() + position,
        std::make_move_iterator(begin), std::make_move_iterator(end));

    return true;
}

bool MotionSearchListModel::Private::commitPrefetch(const QnTimePeriod& periodToCommit)
{
    const auto clearPrefetch = nx::utils::makeScopeGuard([this]() { m_prefetch.clear(); });

    if (currentRequest().direction == FetchDirection::earlier)
        return commitInternal(periodToCommit, m_prefetch.begin(), m_prefetch.end(), count(), false);

    NX_ASSERT(currentRequest().direction == FetchDirection::later);
    return commitInternal(
        periodToCommit, m_prefetch.rbegin(), m_prefetch.rend(), 0, q->effectiveLiveSupported());
}

void MotionSearchListModel::Private::fetchLive()
{
    if (m_liveFetch.id || !q->isLive() || q->livePaused())
        return;

    if (m_data.empty() && fetchInProgress())
        return; //< Don't fetch live if first fetch from archive is in progress.

    const milliseconds from = (m_data.empty() ? 0ms : m_data.front().period.startTime());
    m_liveFetch.period = QnTimePeriod(from.count(), QnTimePeriod::infiniteDuration());
    m_liveFetch.direction = FetchDirection::later;
    m_liveFetch.batchSize = q->fetchBatchSize();

    NX_VERBOSE(q) << "Live update request";
    m_liveFetch.id = getMotion(m_liveFetch.period, Qt::DescendingOrder, m_liveFetch.batchSize);
}

rest::Handle MotionSearchListModel::Private::getMotion(
    const QnTimePeriod& period, Qt::SortOrder order, int limit)
{
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(server && server->apiConnection());
    if (!server || !server->apiConnection())
        return {};

    // TODO: #vkutin This looks like a hack. Think what to do.
    auto regions = m_filterRegions;
    if (q->isFilterEmpty())
    {
        static constexpr QRect kWholeFrame(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight);
        for (auto& region: regions)
            region = kWholeFrame;
    }

    QnChunksRequestData request;
    request.resList = q->cameras().toList();
    request.startTimeMs = period.startTimeMs;
    request.endTimeMs = period.endTimeMs(),
    request.periodsType = Qn::MotionContent;
    request.filter = QJson::serialized(regions);
    request.groupBy = QnChunksRequestData::GroupBy::cameraId;
    request.sortOrder = order;
    request.limit = limit;

    NX_VERBOSE(q) << "Requesting motion periods from"
        << utils::timestampToDebugString(period.startTimeMs) << "to"
        << utils::timestampToDebugString(period.endTimeMs()) << "in"
        << QVariant::fromValue(request.sortOrder).toString()
        << "maximum chunks" << request.limit;

    return server->apiConnection()->recordedTimePeriods(request, this,
        SLOT(processReceivedTimePeriods(int, const MultiServerPeriodDataList &, int)));
}

void MotionSearchListModel::Private::processReceivedTimePeriods(
    int status, const MultiServerPeriodDataList& timePeriods, int requestId)
{
    NX_ASSERT(requestId);
    if (!requestId)
        return;

    std::vector<std::vector<MotionChunk>> chunksByCamera;
    chunksByCamera.resize(timePeriods.size());

    auto source = timePeriods.cbegin();
    for (auto& chunks: chunksByCamera)
    {
        chunks.resize(source->periods.size());
        std::transform(source->periods.cbegin(), source->periods.cend(), chunks.begin(),
            [cameraId = source->guid](const QnTimePeriod& item) -> MotionChunk
            {
                return {cameraId, item};
            });

        ++source;
    }

    const bool success = status == 0;

    if (requestId == currentRequest().id)
    {
        // Archive fetch.

        const auto sortOrder = currentRequest().direction == FetchDirection::earlier
            ? Qt::DescendingOrder
            : Qt::AscendingOrder;

        QnTimePeriod actuallyFetched;
        m_prefetch.clear();

        if (success)
        {
            m_prefetch = nx::utils::algorithm::merge_sorted_lists(std::move(chunksByCamera),
                [](const MotionChunk& chunk) { return chunk.period.startTimeMs; },
                sortOrder, currentRequest().batchSize);

            if (!m_prefetch.empty())
            {
                actuallyFetched = QnTimePeriod::fromInterval(
                    m_prefetch.back().period.startTime(), m_prefetch.front().period.startTime());
            }
        }

        completePrefetch(actuallyFetched, success, int(m_prefetch.size()));
    }
    else if (requestId == m_liveFetch.id)
    {
        // Live fetch.

        const auto scopedClear = nx::utils::makeScopeGuard([this]() { m_liveFetch = {}; });

        if (!success || chunksByCamera.empty() || !q->isLive())
            return;

        auto data = nx::utils::algorithm::merge_sorted_lists(std::move(chunksByCamera),
            [](const MotionChunk& chunk) { return chunk.period.startTimeMs; },
            Qt::DescendingOrder, m_liveFetch.batchSize);

        auto periodToCommit = QnTimePeriod::fromInterval(
            data.back().period.startTime(), data.front().period.startTime());

        if (data.size() >= m_liveFetch.batchSize)
        {
            periodToCommit.truncateFront(periodToCommit.startTimeMs + 1);
            q->clear(); //< Otherwise there will be a gap between live and archive events.
        }

        q->addToFetchedTimeWindow(periodToCommit);

        NX_VERBOSE(q) << "Live update commit";
        commitInternal(periodToCommit, data.begin(), data.end(), 0, true);

        if (count() > q->maximumCount())
        {
            NX_VERBOSE(q) << "Truncating to maximum count";
            truncateToMaximumCount();
        }
    }
}

QSharedPointer<QMenu> MotionSearchListModel::Private::contextMenu(const MotionChunk& chunk) const
{
    if (!q->accessController()->hasGlobalPermission(nx::vms::api::GlobalPermission::manageBookmarks))
        return {};

    const auto camera = this->camera(chunk);
    if (!camera)
        return {};

    QSharedPointer<QMenu> menu(new QMenu());
    menu->addAction(tr("Bookmark it..."),
        [this, camera, period = chunk.period]()
        {
            q->menu()->triggerForced(ui::action::AddCameraBookmarkAction,
                ui::action::Parameters(camera).withArgument(Qn::TimePeriodRole, period));
        });

    return menu;
}

QnVirtualCameraResourcePtr MotionSearchListModel::Private::camera(const MotionChunk& chunk) const
{
    return q->resourcePool()->getResourceById<QnVirtualCameraResource>(chunk.cameraId);
}

} // namespace nx::client::desktop
