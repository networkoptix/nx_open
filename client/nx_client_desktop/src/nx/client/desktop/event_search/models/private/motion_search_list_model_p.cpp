#include "motion_search_list_model_p.h"

#include <QtCore/QTimer>
#include <QtWidgets/QMenu>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/event/event_fwd.h>

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
            return QVariant::fromValue(qnSkin->pixmap("tree/camera.png"));

        case Qn::HelpTopicIdRole:
            return QnBusiness::eventHelpId(vms::api::EventType::cameraMotionEvent);

        case Qn::TimestampRole:
            return QVariant::fromValue(microseconds(chunk.period.startTime()).count());

        case Qn::DurationRole:
            return QVariant::fromValue(microseconds(chunk.period.duration()).count());

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(
                q->resourcePool()->getResourceById<QnVirtualCameraResource>(chunk.cameraId));

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
    const auto dataReceived =
        [this](bool success, rest::Handle requestId, std::vector<MotionChunk>&& data)
        {
            if (!requestId || requestId != currentRequest().id)
                return;

            QnTimePeriod actuallyFetched;
            m_prefetch.clear();

            if (success)
            {
                m_prefetch = std::move(data);
                if (!m_prefetch.empty())
                {
                    actuallyFetched = QnTimePeriod::fromInterval(
                        m_prefetch.back().period.startTime(), m_prefetch.front().period.startTime());
                }
            }

            completePrefetch(actuallyFetched, success, int(m_prefetch.size()));
        };

    const auto sortOrder = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    return getMotion(period, dataReceived, sortOrder, currentRequest().batchSize);
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

    const auto liveEventsReceived =
        [this](bool success, rest::Handle requestId, std::vector<MotionChunk>&& data)
        {
            const auto scopedClear = nx::utils::makeScopeGuard([this]() { m_liveFetch = {}; });

            if (!success || data.empty() || !q->isLive() || !requestId || requestId != m_liveFetch.id)
                return;

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
        };

    NX_VERBOSE(q) << "Live update request";

    m_liveFetch.id = getMotion(m_liveFetch.period, liveEventsReceived, Qt::DescendingOrder,
        m_liveFetch.batchSize);
}

rest::Handle MotionSearchListModel::Private::getMotion(const QnTimePeriod& period,
    GetCallback callback, Qt::SortOrder order, int limit)
{
    // TODO: #vkutin Implement new rest request.
#if 0
    const auto server = q->commonModule()->currentServer();
    NX_ASSERT(callback && server && server->restConnection());
    if (!callback || !server || !server->restConnection())
        return {};

    MotionChunksRequest request;
    if (q->cameraSet()->type() != ManagedCameraSet::Type::all)
    {
        const auto cameras = q->cameraSet()->cameras();
        std::transform(cameras.cbegin(), cameras.cend(), std::back_inserter(request.cameraIds),
            [](const QnVirtualCameraResourcePtr& camera) { return camera->getId(); });
    }

    request.period = period;
    request.filter = m_filterRegions;
    request.limit = limit;
    request.order = order;
    request.detailsPosition = kPreviewTimeFraction;

    NX_VERBOSE(q) << "Requesting motion periods from"
        << utils::timestampToDebugString(period.startTimeMs) << "to"
        << utils::timestampToDebugString(period.endTimeMs()) << "in"
        << QVariant::fromValue(request.order).toString()
        << "maximum objects" << request.limit;

    return server->restConnection()->getMotionPeriods(
        request, false /*isLocal*/, nx::utils::guarded(this, callback), thread());
#endif
    return {};
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
