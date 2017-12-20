#include "bookmark_search_list_model_p.h"

#include <QtGui/QPalette>
#include <QtGui/QPixmap>

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_resource.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>

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

        case Qn::UuidRole:
            return QVariant::fromValue(bookmark.guid);

        case Qn::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(camera());

        case Qn::HelpTopicIdRole:
            return Qn::Bookmarks_Usage_Help;

        default:
            handled = false;
            return QVariant();
    }
}

void BookmarkSearchListModel::Private::clear()
{
    qDebug() << "Clear bookmarks model";

    ScopedReset reset(q, !m_data.empty());
    m_data.clear();
    m_guidToTimestampMs.clear();
    m_prefetch.clear();
    base_type::clear();
}

rest::Handle BookmarkSearchListModel::Private::requestPrefetch(qint64 fromMs, qint64 toMs)
{
    QnCameraBookmarkSearchFilter filter;
    filter.startTimeMs = fromMs;
    filter.endTimeMs = toMs;
    filter.orderBy.column = Qn::BookmarkStartTime;
    filter.orderBy.order = Qt::DescendingOrder;
    filter.limit = kFetchBatchSize;

    qDebug() << "Requesting bookmarks from" << debugTimestampToString(fromMs)
        << "to" << debugTimestampToString(toMs);

    return qnCameraBookmarksManager->getBookmarksAsync({camera()}, filter,
        [this, guard = QPointer<QObject>(this)]
            (bool success, const QnCameraBookmarkList& bookmarks, int requestId)
        {
            if (!guard || shouldSkipResponse(requestId))
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

            complete(m_prefetch.size() < kFetchBatchSize
                ? 0
                : m_prefetch.back().startTimeMs + 1/*discard last ms*/);
        });
}

bool BookmarkSearchListModel::Private::commitPrefetch(qint64 earliestTimeToCommitMs, bool& fetchedAll)
{
    if (!m_success)
    {
        qDebug() << "Committing no bookmarks";
        return false;
    }

    const auto end = std::upper_bound(m_prefetch.cbegin(), m_prefetch.cend(),
        earliestTimeToCommitMs, upperBoundPredicate);

    const auto first = this->count();
    const auto count = std::distance(m_prefetch.cbegin(), end);

    if (count > 0)
    {
        qDebug() << "Committing" << count << "bookmarks from"
            << debugTimestampToString(m_prefetch[count - 1].startTimeMs) << "to"
            << debugTimestampToString(m_prefetch.front().startTimeMs);

        ScopedInsertRows insertRows(q, first, first + count - 1);
        m_data.insert(m_data.end(), m_prefetch.cbegin(), end);

        for (auto iter = m_prefetch.cbegin(); iter != end; ++iter)
            m_guidToTimestampMs[iter->guid] = iter->startTimeMs;
    }
    else
    {
        qDebug() << "Committing no bookmarks";
    }

    fetchedAll = count == m_prefetch.size() && m_prefetch.size() < kFetchBatchSize;
    return true;
}

void BookmarkSearchListModel::Private::clipToSelectedTimePeriod()
{
    const auto itemCleanup =
        [this](const QnCameraBookmark& item) { m_guidToTimestampMs.remove(item.guid); };

    clipToTimePeriod<decltype(m_data), decltype(upperBoundPredicate)>(
        m_data, upperBoundPredicate, selectedTimePeriod(), itemCleanup);
}

bool BookmarkSearchListModel::Private::hasAccessRights() const
{
    return q->accessController()->hasGlobalPermission(Qn::GlobalViewBookmarksPermission);
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
    // Skip bookmarks outside of time range.
    if (!fetchedTimePeriod().contains(bookmark.startTimeMs))
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

} // namespace desktop
} // namespace client
} // namespace nx
