// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "simple_motion_search_list_model.h"

#include <QtCore/QSharedPointer>

// QMenu is the only widget allowed in Right Panel item models.
// It might be refactored later to avoid using QtWidgets at all.
#include <QtWidgets/QMenu>

#include <camera/camera_data_manager.h>
#include <core/resource/camera_resource.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/event_search/models/fetched_data.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/help/rules_help.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/time/formatter.h>
#include <recording/time_period_list.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using nx::vms::client::core::MotionSelection;
using MenuPtr = QSharedPointer<QMenu>;
using namespace nx::vms::api;
using namespace std::chrono;

namespace {

struct Facade
{
    using Type = QnTimePeriod;
    using TimeType = milliseconds;

    static auto startTime(const Type& period)
    {
        return period.startTime();
    }

    static auto id(const Type& period)
    {
        return period.startTimeMs;
    }

    static bool equal(const Type& left, const Type& right)
    {
        return left == right;
    }
};

} // namespace

struct SimpleMotionSearchListModel::Private
{
    SimpleMotionSearchListModel* const q;

    core::CachingCameraDataLoaderPtr loader;
    MotionSelection filterRegions;
    std::deque<QnTimePeriod> data; //< Descending order by start time.

    void updateMotionPeriods(qint64 startTimeMs);

    QnTimePeriodList periods() const; //< Periods from the loader.

    MenuPtr contextMenu(const QnTimePeriod& chunk) const;
};

void SimpleMotionSearchListModel::Private::updateMotionPeriods(qint64 startTimeMs)
{
    if (q->fetchInProgress() || q->isFilterDegenerate())
        return;

    if (data.empty())
    {
        q->clear(); //< Will emit dataNeeded signal.
        return;
    }

    if (const auto interestPeriod = q->interestTimePeriod(); !interestPeriod->contains(startTimeMs))
        return; //< Do not update if new data is outside of interest period.

    q->fetchData({.direction = core::EventSearch::FetchDirection::newer,
        .centralPointUs = microseconds(Facade::startTime(data.at(data.size() / 2)))});
}

QnTimePeriodList SimpleMotionSearchListModel::Private::periods() const
{
    if (!loader)
        return {};

    if (const auto period = q->interestTimePeriod(); period.has_value())
        return loader->periods(Qn::MotionContent).intersected(*period);

    return {};
}

MenuPtr SimpleMotionSearchListModel::Private::contextMenu(const QnTimePeriod& chunk) const
{
    const auto camera = q->navigator()->currentResource().dynamicCast<QnVirtualCameraResource>();
    if (!camera || !ResourceAccessManager::hasPermissions(camera, Qn::ManageBookmarksPermission))
        return {};

    QSharedPointer<QMenu> menu(new QMenu());
    menu->addAction(tr("Bookmark it..."),
        [this, camera, chunk]()
        {
            q->menu()->triggerForced(menu::AddCameraBookmarkAction,
                menu::Parameters(camera).withArgument(Qn::TimePeriodRole, chunk));
        });

    return menu;
}

SimpleMotionSearchListModel::SimpleMotionSearchListModel(WindowContext* context, QObject* parent):
    base_type(context->system(), parent),
    WindowContextAware(context),
    d(new Private{.q = this})
{
    setOfflineAllowed(true);

    connect(navigator(), &QnWorkbenchNavigator::currentResourceChanged,
        this, &AbstractSearchListModel::clear);
}

SimpleMotionSearchListModel::~SimpleMotionSearchListModel()
{
}

MotionSelection SimpleMotionSearchListModel::filterRegions() const
{
    return d->filterRegions;
}

void SimpleMotionSearchListModel::setFilterRegions(const MotionSelection& value)
{
    if (d->filterRegions == value)
        return;

    clear();
    d->filterRegions = value;
    emit filterRegionsChanged();
}

bool SimpleMotionSearchListModel::isFilterEmpty() const
{
    return std::all_of(d->filterRegions.cbegin(), d->filterRegions.cend(),
        [](const QRegion& region)
        {
            return region.isEmpty();
        });
}

void SimpleMotionSearchListModel::clearFilterRegions()
{
    // Clear filter regions. Must keep channel count.
    auto regions = filterRegions();
    for (auto& region: regions)
        region = QRegion();

    setFilterRegions(regions);
}

bool SimpleMotionSearchListModel::isConstrained() const
{
    return !isFilterEmpty() || base_type::isConstrained();
}

bool SimpleMotionSearchListModel::hasAccessRights() const
{
    // Panel is hidden for live viewers but should be visible when browsing local files offline.
    return !isOnline() || system()->accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewArchive);
}

int SimpleMotionSearchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid()
        ? 0
        : d->data.size();
}

QVariant SimpleMotionSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return {};

    const auto& chunk = d->data[index.row()];
    switch (role)
    {
        case Qt::DisplayRole:
            return tr("Motion");

        case Qt::DecorationRole:
            return QVariant::fromValue(qnSkin->pixmap("tree/camera.svg"));

        case Qn::HelpTopicIdRole:
            return rules::eventHelpId(vms::api::EventType::cameraMotionEvent);

        case core::TimestampRole:
            return QVariant::fromValue(chunk.startTime());

        case core::DurationRole:
            return QVariant::fromValue(chunk.duration());

        case core::ResourceRole:
            return QVariant::fromValue(navigator()->currentResource());

        case core::ResourceListRole:
            if (auto resource = navigator()->currentResource())
                return QVariant::fromValue(QnResourceList({resource}));
            return {};

        case core::DescriptionTextRole:
        {
            if (!core::ini().showDebugTimeInformationInEventSearchData)
                return QString();

            // Not translatable debug string.
            return nx::format("Begin: %1<br>End: %2<br>Duration: %3").args(
                nx::utils::timestampToDebugString(chunk.startTimeMs),
                nx::utils::timestampToDebugString(chunk.endTimeMs()),
                text::HumanReadable::timeSpan(chunk.duration())).toQString();
        }

        case core::PreviewTimeRole:
        {
            static constexpr qreal kPreviewFraction = 0.5;

            if (chunk.isInfinite())
                return QVariant::fromValue(microseconds(DATETIME_NOW));

            return QVariant::fromValue(microseconds(milliseconds(
                static_cast<qint64>(chunk.startTimeMs + chunk.durationMs * kPreviewFraction))));
        }

        case Qn::ForcePrecisePreviewRole:
            return true;

        case core::PreviewStreamSelectionRole:
            return QVariant::fromValue(
                nx::api::ImageRequest::StreamSelectionMode::sameAsMotion);

        case Qn::CreateContextMenuRole:
            return QVariant::fromValue(d->contextMenu(chunk));

        default:
            return base_type::data(index, role);
    }
}

void SimpleMotionSearchListModel::clearData()
{
    ScopedReset reset(this, !d->data.empty());

    if (d->loader)
        d->loader->disconnect(this);

    d->data.clear();
    d->loader.reset();

    const auto resource = navigator()->currentResource();
    if (!resource)
        return;

    auto systemContext = SystemContext::fromResource(resource);
    if (!NX_ASSERT(systemContext))
        return;

    if (const auto mediaResource = resource.dynamicCast<QnMediaResource>())
    {
        d->loader = systemContext->cameraDataManager()->loader(
            mediaResource, /*createIfNotExists*/ true);
    }

    if (!d->loader)
        return;

    const auto updatePeriods =
        [this](Qn::TimePeriodContent type, qint64 startTimeMs)
        {
            if (type == Qn::MotionContent)
                d->updateMotionPeriods(startTimeMs);
        };

    connect(d->loader.data(), &core::CachingCameraDataLoader::periodsChanged,
        this, updatePeriods, Qt::DirectConnection);
}

bool SimpleMotionSearchListModel::requestFetch(
    const core::FetchRequest& request,
    const FetchCompletionHandler& completionHandler)
{
    if (!d->loader )
        return false;

    using namespace core;
    using namespace core::event_search;
    using namespace std::chrono;

    auto fetchedPeriods = d->periods();
    if (const auto interestPeriod = interestTimePeriod())
        fetchedPeriods = fetchedPeriods.intersected(*interestPeriod);

    const auto itSplitPoint = std::lower_bound(
        fetchedPeriods.begin(), fetchedPeriods.end(),
        duration_cast<milliseconds>(request.centralPointUs),
        event_search::detail::Predicate<Facade>::lowerBound());

    const int leftPartSize = std::distance(fetchedPeriods.begin(), itSplitPoint);
    const int rightPartSize = std::distance(itSplitPoint, fetchedPeriods.end());
    const int partSize = maximumCount() / 2;

    auto boundedPeriods = std::deque<QnTimePeriod>(
        fetchedPeriods.begin() + std::max(leftPartSize - partSize, 0),
        itSplitPoint + std::min(rightPartSize, partSize));

    if (request.direction == EventSearch::FetchDirection::older)
    {
        // Reverse order since we expect data in descending order for older fetch requests.
        std::reverse(boundedPeriods.begin(), boundedPeriods.end());
    }

    auto fetchedData = makeFetchedData<Facade>(d->data, boundedPeriods, request);
    updateEventSearchData<Facade>(this, d->data, fetchedData, request.direction);
    completionHandler(EventSearch::FetchResult::complete, fetchedData.ranges,
        core::timeWindow<Facade>(d->data));
    return true;
}

bool SimpleMotionSearchListModel::cancelFetch()
{
    return false;
}

bool SimpleMotionSearchListModel::isFilterDegenerate() const
{
    return !d->loader || !navigator()->currentResource()->hasFlags(Qn::motion);
}

} // namespace nx::vms::client::desktop
