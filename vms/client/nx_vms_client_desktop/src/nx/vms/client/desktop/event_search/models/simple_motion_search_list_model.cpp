#include "simple_motion_search_list_model.h"

// QMenu is the only widget allowed in Right Panel item models.
// It might be refactored later to avoid using QtWidgets at all.
#include <QtWidgets/QMenu>

#include <camera/loaders/caching_camera_data_loader.h>
#include <core/resource/camera_resource.h>
#include <ui/help/business_help.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/scoped_value_rollback.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/client/core/utils/human_readable.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>

#include <nx/vms/client/desktop/ini.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

static constexpr qreal kPreviewTimeFraction = 0.5;

microseconds midTime(const QnTimePeriod& period, qreal fraction = kPreviewTimeFraction)
{
    return period.isInfinite()
        ? microseconds(DATETIME_NOW)
        : microseconds(milliseconds(qint64(period.startTimeMs + period.durationMs * fraction)));
}

milliseconds startTime(const QnTimePeriod& period)
{
    return period.startTime();
}

} // namespace

SimpleMotionSearchListModel::SimpleMotionSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, parent)
{
    setOfflineAllowed(true);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &AbstractSearchListModel::clear);
}

QList<QRegion> SimpleMotionSearchListModel::filterRegions() const
{
    return m_filterRegions;
}

void SimpleMotionSearchListModel::setFilterRegions(const QList<QRegion>& value)
{
    if (m_filterRegions == value)
        return;

    clear();
    m_filterRegions = value;
}

bool SimpleMotionSearchListModel::isFilterEmpty() const
{
    return std::all_of(m_filterRegions.cbegin(), m_filterRegions.cend(),
        [](const QRegion& region) { return region.isEmpty(); });
}

bool SimpleMotionSearchListModel::isConstrained() const
{
    return !isFilterEmpty() || base_type::isConstrained();
}

bool SimpleMotionSearchListModel::hasAccessRights() const
{
    return accessController()->hasGlobalPermission(GlobalPermission::viewArchive);
}

int SimpleMotionSearchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_data.size());
}

QVariant SimpleMotionSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return {};

    const auto& chunk = m_data[index.row()];
    switch (role)
    {
        case Qt::DisplayRole:
            return tr("Motion");

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap("tree/camera.svg"));

        case Qn::HelpTopicIdRole:
            return QnBusiness::eventHelpId(vms::api::EventType::cameraMotionEvent);

        case Qn::TimestampRole:
            return QVariant::fromValue(chunk.startTime());

        case Qn::DurationRole:
            return QVariant::fromValue(chunk.duration());

        case Qn::ResourceRole:
            return QVariant::fromValue(navigator()->currentResource());

        case Qn::ResourceListRole:
            if (auto resource = navigator()->currentResource())
                return QVariant::fromValue(QnResourceList({resource}));
            return {};

        case Qn::DescriptionTextRole:
        {
            if (!ini().showDebugTimeInformationInRibbon)
                return QString();

            return lm("Begin: %1<br>End: %2<br>Duration: %3").args( //< Not translatable debug string.
                utils::timestampToDebugString(chunk.startTimeMs),
                utils::timestampToDebugString(chunk.endTimeMs()),
                core::HumanReadable::timeSpan(chunk.duration())).toQString();
        }

        case Qn::PreviewTimeRole:
            return QVariant::fromValue(midTime(chunk));

        case Qn::ForcePrecisePreviewRole:
            return true;

        case Qn::PreviewStreamSelectionRole:
            return QVariant::fromValue(
                nx::api::CameraImageRequest::StreamSelectionMode::sameAsMotion);

        case Qn::ContextMenuRole:
            return QVariant::fromValue(contextMenu(chunk));

        default:
            return base_type::data(index, role);
    }
}

void SimpleMotionSearchListModel::clearData()
{
    ScopedReset reset(this, !m_data.empty());

    if (m_loader)
        m_loader->disconnect(this);

    m_data.clear();

    const auto mediaResource = navigator()->currentResource().dynamicCast<QnMediaResource>();

    m_loader = mediaResource
        ? navigator()->cameraDataManager()->loader(mediaResource, true)
        : QnCachingCameraDataLoaderPtr();

    if (!m_loader)
        return;

    const auto updatePeriods =
        [this](Qn::TimePeriodContent type, qint64 startTimeMs)
        {
            if (type == Qn::MotionContent)
                updateMotionPeriods(startTimeMs);
        };

    connect(m_loader.data(), &QnCachingCameraDataLoader::periodsChanged,
        this, updatePeriods, Qt::DirectConnection);
}

void SimpleMotionSearchListModel::requestFetch()
{
    if (!m_loader || m_fetchInProgress)
        return;

    QnScopedValueRollback<bool> progressRollback(&m_fetchInProgress, true);

    NX_VERBOSE(this, "Fetching more (%1)", QVariant::fromValue(fetchDirection()).toString());

    const auto periods = this->periods();
    const int oldCount = int(m_data.size());
    FetchResult result = FetchResult::complete;

    ScopedFetchCommit scopedFetch(this, fetchDirection(), result /*by reference*/);

    QnTimePeriod fetchedPeriod(relevantTimePeriod());
    if (periods.empty())
    {
        NX_VERBOSE(this, "Loader contains no chunks");
    }
    else
    {
        NX_VERBOSE(this, "Loader contains %1 chunks, time range:\n    from: %2\n    to: %3",
            periods.size(),
            utils::timestampToDebugString(periods.front().startTime()),
            utils::timestampToDebugString(periods.back().startTime()));
    }

    if (fetchDirection() == FetchDirection::earlier)
    {
        const auto begin = m_data.empty()
            ? periods.crbegin()
            : std::upper_bound(periods.crbegin(), periods.crend(), m_data.back().startTime(),
                [](milliseconds left, const QnTimePeriod& right)
                {
                    return left > right.startTime();
                });

        if (!m_data.empty())
            fetchedPeriod.truncate(m_data.back().startTimeMs);

        const int remaining = periods.crend() - begin;
        const int delta = qMin(remaining, fetchBatchSize());
        if (delta < remaining)
        {
            result = FetchResult::incomplete;
            fetchedPeriod.truncateFront(begin->startTimeMs);
        }

        NX_VERBOSE(this, "Fetched %1 chunks", delta);

        addToFetchedTimeWindow(fetchedPeriod);
        if (delta > 0)
        {
            ScopedInsertRows insertRows(this, oldCount, oldCount + delta - 1);
            m_data.insert(m_data.end(), begin, begin + delta);
        }
    }
    else
    {
        NX_ASSERT(fetchDirection() == FetchDirection::later);

        const auto end = m_data.empty()
            ? periods.crend()
            : std::lower_bound(periods.crbegin(), periods.crend(), m_data.front().startTime(),
                [](const QnTimePeriod& left, milliseconds right)
                {
                    return left.startTime() > right;
                });

        if (!m_data.empty())
            fetchedPeriod.truncateFront(m_data.front().startTimeMs + 1);

        if (end != periods.crend() && *end != m_data.front())
            emit dataChanged(index(0), index(0));

        const int remaining = end - periods.crbegin();
        const int delta = qMin(remaining, fetchBatchSize());
        if (delta < remaining)
        {
            result = FetchResult::incomplete;
            fetchedPeriod.truncate((end - 1)->startTimeMs + 1);
        }

        NX_VERBOSE(this, "Fetched %1 chunks", delta);

        addToFetchedTimeWindow(fetchedPeriod);
        if (delta > 0)
        {
            ScopedInsertRows insertRows(this, 0, delta - 1);
            m_data.insert(m_data.begin(), end - delta, end);
        }
    }

    if (rowCount() > maximumCount())
    {
        NX_VERBOSE(this, "Truncating to maximum count");
        truncateToMaximumCount();
    }
}

void SimpleMotionSearchListModel::updateMotionPeriods(qint64 startTimeMs)
{
    if (!NX_ASSERT(!m_fetchInProgress) || isFilterDegenerate())
        return;

    if (m_data.empty())
    {
        clear(); //< Will emit dataNeeded signal.
        return;
    }

    QnScopedValueRollback<bool> progressRollback(&m_fetchInProgress, true);

    // Received startTimeMs covers newly added chunks, but not potentially modified last chunk.
    // This is a workaround.
    if (!m_data.empty())
        startTimeMs = std::clamp(startTimeMs, m_data.back().startTimeMs, m_data.front().startTimeMs);

    NX_VERBOSE(this, "Updating, from %1, old item count = %2",
        utils::timestampToDebugString(startTimeMs),
        m_data.size());

    NX_ASSERT(!fetchedTimeWindow().isNull());

    const auto periods = this->periods();
    const QnTimePeriod updatedPeriod(startTimeMs, QnTimePeriod::kInfiniteDuration);
    const QnTimePeriod periodToUpdate = updatedPeriod.intersected(fetchedTimeWindow());

    static const auto ascendingLowerBoundPredicate =
        [](const QnTimePeriod& left, milliseconds right) { return left.startTime() < right; };

    // `m_data` is sorted by timestamp in descending order, `periods` is sorted in ascending order.

    // We iterate over relevant subparts of both collections in parallel,
    // synchronizing `m_data` with data from `periods`.

    const auto sourceBegin = std::lower_bound(periods.cbegin(), periods.cend(),
        periodToUpdate.startTime(), ascendingLowerBoundPredicate);

    const auto sourceEnd = std::lower_bound(periods.cbegin(), periods.cend(),
        periodToUpdate.endTime(), ascendingLowerBoundPredicate);

    const int updateSize = sourceEnd - sourceBegin;
    if ((updateSize - rowCount()) > maximumCount())
    {
        NX_WARNING(this, "Unexpected update, count = %1, resetting.", updateSize);
        progressRollback.rollback();
        clear(); //< Will emit dataNeeded signal.
        return;
    }

    const auto targetBegin = std::lower_bound(m_data.rbegin(), m_data.rend(),
        periodToUpdate.startTime(), ascendingLowerBoundPredicate);

    // Statistics.
    int numAdded = 0;
    int numRemoved = 0;
    int numUpdated = 0;

    int i = m_data.rend() - targetBegin - 1;
    for (auto source = sourceBegin; source != sourceEnd; ++source, --i)
    {
        if (i < 0 || source->startTime() < m_data[i].startTime())
        {
            ++i;
            ScopedInsertRows insertRows(this, i, i);
            m_data.insert(m_data.begin() + i, *source);
            ++numAdded;
        }
        else if (source->startTime() > m_data[i].startTime())
        {
            ScopedRemoveRows removeRows(this, i, i);
            m_data.erase(m_data.begin() + i);
            ++numRemoved;
        }
        else if (source->duration() != m_data[i].duration())
        {
            const auto modelIndex = index(i);
            m_data[i].durationMs = source->durationMs;
            emit dataChanged(modelIndex, modelIndex);
            ++numUpdated;
        }
    }

    if (i >= 0)
    {
        ScopedRemoveRows removeRows(this, 0, i);
        m_data.erase(m_data.begin(), m_data.begin() + i + 1);
        numRemoved += i + 1;
    }

    NX_VERBOSE(this, "Added %1, removed %2, updated %3 chunks", numAdded, numRemoved, numUpdated);

    if (!m_data.empty())
    {
        setFetchedTimeWindow(QnTimePeriod::fromInterval(
            m_data.back().startTime(), m_data.front().startTime()));
    }

    if (rowCount() > maximumCount())
    {
        NX_VERBOSE(this, "Truncating to maximum count");
        truncateToMaximumCount();
    }
    else if (rowCount() < fetchBatchSize())
    {
        progressRollback.rollback();
        requestFetch();
    }
}

bool SimpleMotionSearchListModel::isFilterDegenerate() const
{
    return !m_loader || !navigator()->currentResource()->hasFlags(Qn::motion);
}

void SimpleMotionSearchListModel::truncateToRelevantTimePeriod()
{
    this->truncateDataToTimePeriod(m_data, &startTime, relevantTimePeriod());
}

void SimpleMotionSearchListModel::truncateToMaximumCount()
{
    this->truncateDataToMaximumCount(m_data, &startTime);
}

QnTimePeriodList SimpleMotionSearchListModel::periods() const
{
    return m_loader
        ? m_loader->periods(Qn::MotionContent).intersected(relevantTimePeriod())
        : QnTimePeriodList();
}

QSharedPointer<QMenu> SimpleMotionSearchListModel::contextMenu(const QnTimePeriod& chunk) const
{
    if (!accessController()->hasGlobalPermission(nx::vms::api::GlobalPermission::manageBookmarks))
        return {};

    const auto camera = navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return {};

    QSharedPointer<QMenu> menu(new QMenu());
    menu->addAction(tr("Bookmark it..."),
        [this, camera, chunk]()
        {
            this->menu()->triggerForced(ui::action::AddCameraBookmarkAction,
                ui::action::Parameters(camera).withArgument(Qn::TimePeriodRole, chunk));
        });

    return menu;
}

} // namespace nx::vms::client::desktop
