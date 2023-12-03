// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_search_list_model.h"

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/common/bookmark/bookmark_facade.h>
#include <nx/vms/common/html/html.h>
#include <nx/utils/algorithm/comparator.h>
#include <nx/utils/math/math.h>

#include "detail/multi_request_id_holder.h"

namespace nx::vms::client::core {

using namespace std::chrono;

namespace {

const auto sPredicate = nx::utils::algorithm::Comparator(
    /*ascending*/ false, &QnCameraBookmark::startTimeMs, &QnCameraBookmark::guid);

std::set<QnUuid> idsFromCameras(const QnVirtualCameraResourceSet& cameras)
{
    std::set<QnUuid> result;
    for (const auto& camera: cameras)
        result.insert(camera->getId());
    return result;
}

QString parseHyperlinks(const QString& text)
{
    if (text.isEmpty())
        return "";

    QString result;
    static const QString urlPattern =
        R"(\b((http(s)?:\/\/|www\.)([-A-Z0-9+&@#\/%=~_|$\?!:,.])*([A-Z0-9+&@#\/%=~_|$])))";

    QRegularExpression rx(urlPattern, QRegularExpression::CaseInsensitiveOption);

    int pos = 0;
    int lastPos = 0;

    auto it = rx.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();

        pos = match.capturedStart();
        result.append(text.mid(lastPos, pos - lastPos));
        lastPos = pos;
        pos += match.capturedLength();
        QString linkText = text.mid(lastPos, pos - lastPos);
        QString link = linkText;
        if (link.startsWith("www."))
            link.prepend("https://");
        result.append(nx::vms::common::html::customLink(linkText, link));
        lastPos = pos;
    }

    result.append(text.mid(lastPos));
    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

struct BookmarkSearchListModel::Private
{
    BookmarkSearchListModel* const q;

    const std::unique_ptr<TextFilterSetup> textFilter{new TextFilterSetup()};

    QnCameraBookmarkList bookmarks;

    detail::MultiRequestIdHolder requestIds;

    QnVirtualCameraResourcePtr camera(const QnCameraBookmark& bookmark) const;

    void addBookmark(const QnCameraBookmark& bookmark);
    void updateBookmark(const QnCameraBookmark& bookmark);
    void removeBookmark(const QnUuid& id);

    bool requestFetch(
        const FetchRequest& request,
        const FetchCompletionHandler& completionHandler,
        detail::MultiRequestIdHolder::Mode mode);
};

QnVirtualCameraResourcePtr BookmarkSearchListModel::Private::camera(
    const QnCameraBookmark& bookmark) const
{
    const auto user = q->userWatcher()->user();
    return user
        ? q->resourcePool()->getResourceById<QnVirtualCameraResource>(bookmark.cameraId)
        : QnVirtualCameraResourcePtr();
}

void BookmarkSearchListModel::Private::addBookmark(const QnCameraBookmark& bookmark)
{
    // Skip bookmarks outside of time range.
    const auto currentWindow = q->fetchedTimeWindow();
    if (currentWindow && !currentWindow->contains(bookmark.startTimeMs.count()))
        return;

    const auto it = std::lower_bound(bookmarks.cbegin(), bookmarks.cend(), bookmark, sPredicate);
    const auto index = std::distance(bookmarks.cbegin(), it);

    const ScopedInsertRows insertRowsGuard(q, index, index);
    bookmarks.insert(bookmarks.begin() + index, bookmark);
}

void BookmarkSearchListModel::Private::updateBookmark(const QnCameraBookmark& bookmark)
{
    const auto it = std::lower_bound(bookmarks.begin(), bookmarks.end(), bookmark, sPredicate);
    if (it == bookmarks.end() || it->guid != bookmark.guid)
        return;

    if (!NX_ASSERT(it->startTimeMs == bookmark.startTimeMs))
    {
        // Normally bookmark timestamp should not change, but handle it anyway.
        removeBookmark(bookmark.guid);
        addBookmark(bookmark);
        return;
    }

    const auto index = std::distance(bookmarks.begin(), it);
    const auto modelIndex = q->index(index);
    *it = bookmark;

    emit q->dataChanged(modelIndex, modelIndex);
}

void BookmarkSearchListModel::Private::removeBookmark(const QnUuid& id)
{
    const auto it = std::find_if(bookmarks.begin(), bookmarks.end(),
        [id](const QnCameraBookmark& bookmark) { return bookmark.guid == id; });

    if (it == bookmarks.end())
        return;

    const auto index = std::distance(bookmarks.begin(), it);
    const ScopedRemoveRows removeRowsGuard(q, index, index);
    bookmarks.erase(it);
}

bool BookmarkSearchListModel::Private::requestFetch(
    const FetchRequest& request,
    const FetchCompletionHandler& completionHandler,
    detail::MultiRequestIdHolder::Mode mode)
{
    using namespace nx::vms::api;

    if (requestIds.value(mode))
        return false; //< Corresponding request is in progress.

    const auto sortOrder = EventSearch::sortOrderFromDirection(request.direction);
    auto filter = QnCameraBookmarkSearchFilter{
        .centralTimePointMs = duration_cast<milliseconds>(request.centralPointUs),
        .limit = q->maximumCount(),
        .orderBy = QnBookmarkSortOrder(BookmarkSortField::startTime, sortOrder),
        .cameras = idsFromCameras(q->cameraSet().cameras())};

    if (const auto& interestTimePeriod = q->interestTimePeriod())
    {
        filter.startTimeMs = interestTimePeriod->startTime();
        filter.endTimeMs = interestTimePeriod->endTime();
    }

    const auto bookmarksManager = q->cameraBookmarksManager();

    const auto safeCompletionHandler =
        [completionHandler](const auto result, const auto& ranges, const auto& timeWindow)
        {
            if (completionHandler)
                completionHandler(result, ranges, timeWindow);
        };

    return !!requestIds.setValue(mode, bookmarksManager->getBookmarksAroundPointAsync(
        filter, bookmarks,
        [this, mode, request, completionHandler = safeCompletionHandler]
            (bool success, rest::Handle id, BookmarkFetchedData&& data)
        {
            if (requestIds.value(mode) != id)
                return;

            requestIds.resetValue(mode);

            const auto applyFetchedData =
                [this, direction = request.direction](BookmarkFetchedData&& fetched,
                    const OptionalTimePeriod& timePeriod)
                {
                    // Explicitly update fetched time window in case of dynamic update.
                    q->setFetchedTimeWindow(timePeriod);
                    updateEventSearchData<nx::vms::common::QnBookmarkFacade>(
                        q, bookmarks, fetched, direction);
                };

            if (!success)
            {
                applyFetchedData({}, {});
                completionHandler(EventSearch::FetchResult::failed,
                    FetchedDataRanges{},
                    QnTimePeriod{});
                return;
            }

            const auto ranges = data.ranges;
            const auto fetchedWindow = timeWindow<nx::vms::common::QnBookmarkFacade>(data.data);
            applyFetchedData(std::move(data), fetchedWindow);
            completionHandler(EventSearch::FetchResult::complete, ranges, fetchedWindow);

            q->setFetchedTimeWindow(fetchedWindow);
        }));
}

//-------------------------------------------------------------------------------------------------

BookmarkSearchListModel::BookmarkSearchListModel(
    int maximumCount,
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(maximumCount, systemContext, parent),
    d(new Private{.q = this})
{
    const auto bookmarksManager = systemContext->cameraBookmarksManager();
    connect(bookmarksManager, &QnCameraBookmarksManager::bookmarkAdded, this,
        [this](const auto& bookmark) { d->addBookmark(bookmark); });
    connect(bookmarksManager, &QnCameraBookmarksManager::bookmarkUpdated, this,
        [this](const auto& bookmark) { d->updateBookmark(bookmark); });
    connect(bookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved, this,
        [this](const auto& id) { d->removeBookmark(id); });
}

BookmarkSearchListModel::BookmarkSearchListModel(
    SystemContext* systemContext,
    QObject* parent)
    :
    BookmarkSearchListModel(kStandardMaximumItemCount, systemContext, parent)
{
}

BookmarkSearchListModel::~BookmarkSearchListModel()
{
}

TextFilterSetup* BookmarkSearchListModel::textFilter() const
{
    return d->textFilter.get();
}

bool BookmarkSearchListModel::isConstrained() const
{
    return !d->textFilter->text().isEmpty() || base_type::isConstrained();
}

bool BookmarkSearchListModel::hasAccessRights() const
{
    return systemContext()->accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewBookmarks);
}

void BookmarkSearchListModel::dynamicUpdate(const qint64& centralPointMs)
{
    d->requestFetch({
        .direction = EventSearch::FetchDirection::newer,
        .centralPointUs = milliseconds(centralPointMs)},
        {}, detail::MultiRequestIdHolder::Mode::dynamic);
}

int BookmarkSearchListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid()
        ? 0
        : d->bookmarks.size();
}

QVariant BookmarkSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(qBetween<int>(0, index.row(), d->bookmarks.size())))
        return {};

    const auto& bookmark = d->bookmarks[index.row()];

    switch (role)
    {
        case Qt::DisplayRole:
            return bookmark.name;

        case DescriptionTextRole:
            return parseHyperlinks(nx::vms::common::html::toHtml(bookmark.description));

        case TimestampRole:
        case PreviewTimeRole:
            return QVariant::fromValue(duration_cast<microseconds>(bookmark.startTimeMs));

        case TimestampMsRole:
            return QVariant::fromValue(bookmark.startTimeMs.count());

        case DurationRole:
            return QVariant::fromValue(bookmark.durationMs);

        case DurationMsRole:
            return QVariant::fromValue(duration_cast<milliseconds>(bookmark.durationMs).count());

        case UuidRole:
            return QVariant::fromValue(bookmark.guid);

        case ResourceListRole:
        case DisplayedResourceListRole:
        {
            if (const auto resource = d->camera(bookmark))
                return QVariant::fromValue(QnResourceList({resource}));

            if (role == DisplayedResourceListRole)
                return QVariant::fromValue(QStringList({QString("<%1>").arg(tr("deleted camera"))}));

            return {};
        }

        case ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(d->camera(bookmark));

        case RawResourceRole:
        {
            const auto resource = d->camera(bookmark);
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        case CameraBookmarkRole:
            return QVariant::fromValue(bookmark);

        case BookmarkTagRole:
            return QVariant::fromValue(bookmark.tags);

        default:
            return {};
    }
}

bool BookmarkSearchListModel::requestFetch(
    const FetchRequest& request,
    const FetchCompletionHandler& completionHandler)
{
    if (currentRequest())
        return false; //< Can't make a new request until the old one is in progress.

    // Cancel dynamic update request.
    d->requestIds.resetValue(detail::MultiRequestIdHolder::Mode::dynamic);

    return d->requestFetch(request, completionHandler, detail::MultiRequestIdHolder::Mode::fetch);
}

void BookmarkSearchListModel::clearData()
{
    ScopedReset reset(this, !d->bookmarks.empty());
    d->bookmarks.clear();
}

} // namespace nx::vms::client::core
