#include "bookmark_search_list_model_p.h"

#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <camera/camera_bookmarks_manager.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/synctime.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kFetchBatchSize = 25;

static const auto lowerBoundPredicate =
    [](const QnCameraBookmark& left, qint64 right) { return left.startTimeMs > right; };

static const auto upperBoundPredicate =
    [](qint64 left, const QnCameraBookmark& right) { return left > right.startTimeMs; };

} // namespace

BookmarkSearchListModel::Private::Private(BookmarkSearchListModel* q):
    base_type(),
    q(q)
{
    watchBookmarkChanges();
}

BookmarkSearchListModel::Private::~Private()
{
}

QnVirtualCameraResourcePtr BookmarkSearchListModel::Private::camera() const
{
    return m_camera;
}

void BookmarkSearchListModel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    clear();
    m_camera = camera;
}

int BookmarkSearchListModel::Private::count() const
{
    return int(m_data.size());
}

const QnCameraBookmark& BookmarkSearchListModel::Private::bookmark(int index) const
{
    return m_data[index];
}

void BookmarkSearchListModel::Private::clear()
{
    qDebug() << "Clear bookmarks model";

    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_guidToTimestampMs.clear();
    m_prefetch.clear();
    m_prefetchCompletionHandler = PrefetchCompletionHandler();
    m_fetchedAll = false;
    m_earliestTimeMs = std::numeric_limits<qint64>::max();
    m_currentFetchId = 0;
}

bool BookmarkSearchListModel::Private::canFetchMore() const
{
    return !m_fetchedAll && m_camera && !fetchInProgress()
        && q->accessController()->hasGlobalPermission(Qn::GlobalViewBookmarksPermission);
}

bool BookmarkSearchListModel::Private::prefetch(PrefetchCompletionHandler completionHandler)
{
    if (!canFetchMore() || !completionHandler)
        return false;

    QnCameraBookmarkSearchFilter filter;
    filter.startTimeMs = 0;
    filter.endTimeMs = m_earliestTimeMs - 1;
    filter.orderBy.column = Qn::BookmarkStartTime;
    filter.orderBy.order = Qt::DescendingOrder;
    filter.limit = kFetchBatchSize;

    qDebug() << "Requesting bookmarks from 0 to" << (
        m_earliestTimeMs != std::numeric_limits<qint64>::max()
            ? debugTimestampToString(m_earliestTimeMs - 1)
            : lit("infinity"));

    m_currentFetchId = qnCameraBookmarksManager->getBookmarksAsync({m_camera}, filter,
        [this, guard = QPointer<QObject>(this)]
            (bool success, const QnCameraBookmarkList& bookmarks, int requestId)
        {
            if (!guard || requestId != m_currentFetchId)
                return;

            NX_ASSERT(m_prefetch.empty() && m_prefetchCompletionHandler);
            if (!m_prefetchCompletionHandler)
                return;

            m_prefetch = success ? std::move(bookmarks) : QnCameraBookmarkList();
            m_success = success;

            if (m_prefetch.empty())
            {
                qDebug() << "Pre-fetched no bookmarks";
            }
            else
            {
                qDebug() << "Pre-fetched" << m_prefetch.size() << "bookmarks from"
                    << debugTimestampToString(m_prefetch.back().startTimeMs) << "to"
                    << debugTimestampToString(m_prefetch.front().startTimeMs);
            }

            m_prefetchCompletionHandler(m_prefetch.size() < kFetchBatchSize
                ? 0
                : m_prefetch.back().startTimeMs + 1/*discard last ms*/);

            m_prefetchCompletionHandler = PrefetchCompletionHandler();
        });

    if (!m_currentFetchId)
        return false;

    m_prefetchCompletionHandler = completionHandler;
    return true;
}

void BookmarkSearchListModel::Private::commitPrefetch(qint64 latestStartTimeMs)
{
    if (!m_currentFetchId)
        return;

    m_currentFetchId = 0;

    if (!m_success)
    {
        qDebug() << "Committing no bookmarks";
        return;
    }

    const auto end = std::upper_bound(m_prefetch.cbegin(), m_prefetch.cend(),
        latestStartTimeMs, upperBoundPredicate);

    const auto first = this->count();
    const auto count = std::distance(m_prefetch.cbegin(), end);

    if (count > 0)
    {
        qDebug() << "Committing" << count << "bookmarks from"
            << debugTimestampToString(m_prefetch[count - 1].startTimeMs) << "to"
            << debugTimestampToString(m_prefetch.front().startTimeMs);

        ScopedInsertRows insertRows(q,  first, first + count - 1);
        m_data.insert(m_data.end(), m_prefetch.cbegin(), end);

        for (auto iter = m_prefetch.cbegin(); iter != end; ++iter)
            m_guidToTimestampMs[iter->guid] = iter->startTimeMs;
    }
    else
    {
        qDebug() << "Committing no bookmarks";
    }

    m_fetchedAll = count == m_prefetch.size() && m_prefetch.size() < kFetchBatchSize;
    m_earliestTimeMs = latestStartTimeMs;
    m_prefetch.clear();
}

bool BookmarkSearchListModel::Private::fetchInProgress() const
{
    return m_currentFetchId != 0;
}

void BookmarkSearchListModel::Private::watchBookmarkChanges()
{
    // TODO: #vkutin Check whether qnCameraBookmarksManager won't emit these signals
    // if current user has no Qn::GlobalViewBookmarksPermission

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkAdded,
        this, &Private::addBookmark);

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkUpdated,
        this, &Private::updateBookmark);

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved,
        this, &Private::removeBookmark);
}

void BookmarkSearchListModel::Private::addBookmark(const QnCameraBookmark& bookmark)
{
    if (bookmark.startTimeMs < m_earliestTimeMs) //< Skip bookmarks outside of time range.
        return;

    if (m_guidToTimestampMs.contains(bookmark.guid))
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
    m_guidToTimestampMs[bookmark.guid] = bookmark.startTimeMs;
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
    m_guidToTimestampMs.remove(guid);
}

int BookmarkSearchListModel::Private::indexOf(const QnUuid& guid) const
{
    const auto iter = m_guidToTimestampMs.find(guid);
    if (iter == m_guidToTimestampMs.end())
        return -1;

    const auto range = std::make_pair(
        std::lower_bound(m_data.cbegin(), m_data.cend(), iter.value(), lowerBoundPredicate),
        std::upper_bound(m_data.cbegin(), m_data.cend(), iter.value(), upperBoundPredicate));

    const auto pos = std::find_if(range.first, range.second,
        [&guid](const QnCameraBookmark& item) { return item.guid == guid; });

    return pos != range.second ? std::distance(m_data.cbegin(), pos) : -1;
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
            qnSkin->pixmap(lit("buttons/acknowledge.png")), bookmarkColor);
    }

    return bookmarkPixmap;
}

QColor BookmarkSearchListModel::Private::color()
{
    return QPalette().color(QPalette::LinkVisited);
}

} // namespace
} // namespace client
} // namespace nx
