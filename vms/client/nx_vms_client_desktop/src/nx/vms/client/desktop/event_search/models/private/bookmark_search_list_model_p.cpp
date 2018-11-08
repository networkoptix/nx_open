#include "bookmark_search_list_model_p.h"

#include <chrono>

#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;
using namespace std::literals::chrono_literals;

static const auto lowerBoundPredicate =
    [](const QnCameraBookmark& left, milliseconds right) { return left.startTimeMs > right; };

static const auto upperBoundPredicate =
    [](milliseconds left, const QnCameraBookmark& right) { return left > right.startTimeMs; };

} // namespace

BookmarkSearchListModel::Private::Private(BookmarkSearchListModel* q):
    base_type(q),
    q(q)
{
    watchBookmarkChanges();
}

BookmarkSearchListModel::Private::~Private()
{
}

int BookmarkSearchListModel::Private::count() const
{
    return int(m_data.size());
}

QVariant BookmarkSearchListModel::Private::data(const QModelIndex& index, int role,
    bool& handled) const
{
    const auto& bookmark = m_data[index.row()];
    handled = true;

    switch (role)
    {
        case Qt::DisplayRole:
            return bookmark.name;

        case Qt::DecorationRole:
            return QVariant::fromValue(pixmap());

        case Qt::ForegroundRole:
            return QVariant::fromValue(color());

        case Qn::DescriptionTextRole:
            return bookmark.description;

        case Qn::TimestampRole:
        case Qn::PreviewTimeRole:
            return QVariant::fromValue(bookmark.startTimeMs);

        case Qn::DurationRole:
            return QVariant::fromValue(bookmark.durationMs);

        case Qn::UuidRole:
            return QVariant::fromValue(bookmark.guid);

        case Qn::ResourceListRole:
            return QVariant::fromValue(QnResourceList({camera(bookmark)}));

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera(bookmark));

        case Qn::HelpTopicIdRole:
            return Qn::Bookmarks_Usage_Help;

        default:
            handled = false;
            return QVariant();
    }
}

QString BookmarkSearchListModel::Private::filterText() const
{
    return m_filterText;
}

void BookmarkSearchListModel::Private::setFilterText(const QString& value)
{
    if (m_filterText == value)
        return;

    q->clear();
    m_filterText = value;
}

void BookmarkSearchListModel::Private::clearData()
{
    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_guidToTimestamp.clear();
    m_prefetch.clear();
}

void BookmarkSearchListModel::Private::truncateToMaximumCount()
{
    const auto itemCleanup =
        [this](const QnCameraBookmark& item) { m_guidToTimestamp.remove(item.guid); };

    this->truncateDataToMaximumCount(m_data,
        [](const QnCameraBookmark& item) { return item.startTimeMs; },
        itemCleanup);
}

void BookmarkSearchListModel::Private::truncateToRelevantTimePeriod()
{
    const auto itemCleanup =
        [this](const QnCameraBookmark& item) { m_guidToTimestamp.remove(item.guid); };

    this->truncateDataToTimePeriod(
        m_data, upperBoundPredicate, q->relevantTimePeriod(), itemCleanup);
}

rest::Handle BookmarkSearchListModel::Private::requestPrefetch(const QnTimePeriod& period)
{
    QnCameraBookmarkSearchFilter filter;
    filter.startTimeMs = period.startTime();
    filter.endTimeMs = period.endTime();
    filter.text = m_filterText;
    filter.orderBy.column = Qn::BookmarkStartTime;
    filter.orderBy.order = currentRequest().direction == FetchDirection::earlier
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;

    filter.limit = currentRequest().batchSize;

    NX_VERBOSE(q) << "Requesting bookmarks from"
        << utils::timestampToDebugString(period.startTimeMs) << "to"
        << utils::timestampToDebugString(period.endTimeMs()) << "in"
        << QVariant::fromValue(filter.orderBy.order).toString()
        << "maximum count" << filter.limit;

    const auto callback =
        [this](bool success, const QnCameraBookmarkList& bookmarks, int requestId)
        {
            if (!requestId || requestId != currentRequest().id)
                return;

            QnTimePeriod actuallyFetched;
            m_prefetch = QnCameraBookmarkList();

            if (success)
            {
                m_prefetch = std::move(bookmarks);
                if (!m_prefetch.empty())
                {
                    actuallyFetched = QnTimePeriod::fromInterval(
                        m_prefetch.back().startTimeMs, m_prefetch.front().startTimeMs);
                }
            }

            completePrefetch(actuallyFetched, success, m_prefetch.size());
        };

    return qnCameraBookmarksManager->getBookmarksAsync(q->cameras(), filter,
        BookmarksInternalCallbackType(nx::utils::guarded(this, callback)));
}

template<typename Iter>
bool BookmarkSearchListModel::Private::commitPrefetch(
    const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd, int position)
{
    const auto clearPrefetch = nx::utils::makeScopeGuard([this]() { m_prefetch.clear(); });

    const auto begin = std::lower_bound(prefetchBegin, prefetchEnd,
        periodToCommit.endTime(), lowerBoundPredicate);

    const auto end = std::upper_bound(prefetchBegin, prefetchEnd,
        periodToCommit.startTime(), upperBoundPredicate);

    const auto count = std::distance(begin, end);
    if (count <= 0)
    {
        NX_VERBOSE(q) << "Committing no bookmarks";
        return false;
    }

    NX_VERBOSE(q) << "Committing" << count << "bookmarks from"
        << utils::timestampToDebugString(((end - 1)->startTimeMs).count()) << "to"
        << utils::timestampToDebugString((begin->startTimeMs).count());

    ScopedInsertRows insertRows(q, position, position + count - 1);
    m_data.insert(m_data.begin() + position, begin, end);

    for (auto iter = begin; iter != end; ++iter)
        m_guidToTimestamp[iter->guid] = iter->startTimeMs;

    return true;
}

bool BookmarkSearchListModel::Private::commitPrefetch(const QnTimePeriod& periodToCommit)
{
    if (currentRequest().direction == FetchDirection::earlier)
        return commitPrefetch(periodToCommit, m_prefetch.cbegin(), m_prefetch.cend(), count());

    NX_ASSERT(!q->liveSupported()); //< We don't handle overlaps as this model is not live-updated.
    NX_ASSERT(currentRequest().direction == FetchDirection::later);
    return commitPrefetch(periodToCommit, m_prefetch.crbegin(), m_prefetch.crend(), 0);
}

bool BookmarkSearchListModel::Private::hasAccessRights() const
{
    return q->accessController()->hasGlobalPermission(GlobalPermission::viewBookmarks);
}

void BookmarkSearchListModel::Private::watchBookmarkChanges()
{
    // TODO: #vkutin Check whether qnCameraBookmarksManager won't emit these signals
    // if current user has no GlobalPermission::viewBookmarks

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkAdded,
        this, &Private::addBookmark);

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkUpdated,
        this, &Private::updateBookmark);

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved,
        this, &Private::removeBookmark);
}

void BookmarkSearchListModel::Private::addBookmark(const QnCameraBookmark& bookmark)
{
    // Skip bookmarks outside of time range.
    if (!q->fetchedTimeWindow().contains(bookmark.startTimeMs.count()))
        return;

    if (m_guidToTimestamp.contains(bookmark.guid))
    {
        NX_ASSERT(false, Q_FUNC_INFO, "Bookmark already exists");
        updateBookmark(bookmark);
        return;
    }

    const auto insertionPos = std::lower_bound(m_data.cbegin(), m_data.cend(),
        bookmark.startTimeMs, lowerBoundPredicate);

    const auto index = std::distance(m_data.cbegin(), insertionPos);

    ScopedInsertRows insertRows(q,  index, index);
    m_data.insert(m_data.begin() + index, bookmark);
    m_guidToTimestamp[bookmark.guid] = bookmark.startTimeMs;
}

void BookmarkSearchListModel::Private::updateBookmark(const QnCameraBookmark& bookmark)
{
    const auto index = indexOf(bookmark.guid);
    if (index < 0)
        return;

    if (m_data[index].startTimeMs == bookmark.startTimeMs)
    {
        // Update data.
        const auto modelIndex = q->index(index);
        m_data[index] = bookmark;
        emit q->dataChanged(modelIndex, modelIndex);
    }
    else
    {
        // Normally bookmark timestamp should not change, but handle it anyway.
        removeBookmark(bookmark.guid);
        addBookmark(bookmark);
    }
}

void BookmarkSearchListModel::Private::removeBookmark(const QnUuid& guid)
{
    const auto index = indexOf(guid);
    if (index < 0)
        return;

    ScopedRemoveRows removeRows(q,  index, index);
    m_data.erase(m_data.begin() + index);
    m_guidToTimestamp.remove(guid);
}

int BookmarkSearchListModel::Private::indexOf(const QnUuid& guid) const
{
    const auto iter = m_guidToTimestamp.find(guid);
    if (iter == m_guidToTimestamp.end())
        return -1;

    const auto range = std::make_pair(
        std::lower_bound(m_data.cbegin(), m_data.cend(), iter.value(), lowerBoundPredicate),
        std::upper_bound(m_data.cbegin(), m_data.cend(), iter.value(), upperBoundPredicate));

    const auto pos = std::find_if(range.first, range.second,
        [&guid](const QnCameraBookmark& item) { return item.guid == guid; });

    return pos != range.second ? std::distance(m_data.cbegin(), pos) : -1;
}

QnVirtualCameraResourcePtr BookmarkSearchListModel::Private::camera(
    const QnCameraBookmark& bookmark) const
{
    return q->resourcePool()->getResourceById<QnVirtualCameraResource>(bookmark.cameraId);
}

// TODO: #vkutin Make color customized properly. Replace icon with pre-colorized one.
QPixmap BookmarkSearchListModel::Private::pixmap()
{
    static QColor bookmarkColor;
    static QPixmap bookmarkPixmap;

    const auto color = Private::color();
    if (bookmarkColor != color)
    {
        bookmarkColor = color;
        bookmarkPixmap = QnSkin::colorize(
            qnSkin->pixmap("buttons/acknowledge.png"), bookmarkColor);
    }

    return bookmarkPixmap;
}

QColor BookmarkSearchListModel::Private::color()
{
    return QPalette().color(QPalette::Light);
}

} // namespace nx::vms::client::desktop
