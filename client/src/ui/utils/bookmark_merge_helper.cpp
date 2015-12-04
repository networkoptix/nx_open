#include "bookmark_merge_helper.h"

#include <boost/optional/optional.hpp>

#include <core/resource/camera_bookmark.h>

namespace {

struct BookmarkItem;
typedef QSharedPointer<BookmarkItem> BookmarkItemPtr;
typedef QList<BookmarkItemPtr> BookmarkItemList;

struct BookmarkItem
{
    boost::optional<QnCameraBookmark> bookmark;

    BookmarkItemPtr parent;

    qint64 startTimeMs;
    qint64 endTimeMs;

public:
    BookmarkItem()
        : startTimeMs(0)
        , endTimeMs(0)
    {
    }

    BookmarkItem(const QnCameraBookmark &bookmark)
        : bookmark(bookmark)
        , startTimeMs(bookmark.startTimeMs)
        , endTimeMs(bookmark.endTimeMs())
    {
    }

    QnTimelineBookmarkItem toTimelineBookmarkItem() const;
    bool isParentedBy(const BookmarkItemPtr &parent) const;
};

struct DetailLevel
{
    qint64 msecsPerDpix;
    QList<BookmarkItemPtr> items;

    DetailLevel(int msecsInDpix)
        : msecsPerDpix(msecsInDpix)
    {
    }
};

} // anonymous namespace

class QnBookmarkMergeHelperPrivate
{
public:
    int bookmarkMergeDistanceDpix;
    int bookmarkMergeSpacingDpix;
    QList<DetailLevel> detailLevels;

    QnBookmarkMergeHelperPrivate()
        : bookmarkMergeDistanceDpix(100)
        , bookmarkMergeSpacingDpix(20)
    {
    }

public:
    int detailLevel(qint64 msecsPerDPix) const;
    void clear();
    void mergeBookmarkItems(int detailLevel);
    void adjustItemBoundsAfterRemoval(BookmarkItemPtr item, int level, const BookmarkItemPtr &removedItem);
    BookmarkItemList bookmarksForItem(const BookmarkItemPtr &item) const;
};

QnBookmarkMergeHelper::QnBookmarkMergeHelper() :
    d_ptr(new QnBookmarkMergeHelperPrivate)
{
    Q_D(QnBookmarkMergeHelper);
    d->detailLevels << DetailLevel(0) // Zero level - not merged
                    << DetailLevel(1000)            // 1 sec
                    << DetailLevel(5 * 1000)        // 5 sec
                    << DetailLevel(15 * 1000)       // 15 sec
                    << DetailLevel(30 * 1000)       // 30 sec
                    << DetailLevel(60 * 1000)       // 1 min
                    << DetailLevel(5 * 60 * 1000)   // 5 min
                    << DetailLevel(15 * 60 * 1000)  // 15 min
                    << DetailLevel(30 * 60 * 1000)  // 30 min
                    << DetailLevel(60 * 60 * 1000)          // 1 hr
                    << DetailLevel(3 * 60 * 60 * 1000)      // 3 hr
                    << DetailLevel(6 * 60 * 60 * 1000)      // 6 hr
                    << DetailLevel(12 * 60 * 60 * 1000)     // 12 hr
                    << DetailLevel(24 * 60 * 60 * 1000)         // 1 day
                    << DetailLevel(5 * 24 * 60 * 60 * 1000)     // 5 days
                    << DetailLevel(15 * 24 * 60 * 60 * 1000)    // 15 days
                    << DetailLevel(30 * 24 * 60 * 60 * 1000)        // 1 month
                    << DetailLevel(3 * 30 * 24 * 60 * 60 * 1000);   // 3 months
}

QnBookmarkMergeHelper::~QnBookmarkMergeHelper()
{
}

QnTimelineBookmarkItemList QnBookmarkMergeHelper::bookmarks(qint64 msecsPerDp) const
{
    Q_D(const QnBookmarkMergeHelper);

    QnTimelineBookmarkItemList result;
    for (const BookmarkItemPtr &item: d->detailLevels[d->detailLevel(msecsPerDp)].items)
        result.append(item->toTimelineBookmarkItem());

    return result;
}

QnCameraBookmarkList QnBookmarkMergeHelper::bookmarksAtPosition(qint64 timeMs, int msecsPerDp) const
{
    Q_D(const QnBookmarkMergeHelper);

    const DetailLevel &level = d->detailLevels[d->detailLevel(msecsPerDp)];

    QnCameraBookmarkList result;

    for (const BookmarkItemPtr &item: level.items) {
        if (item->startTimeMs > timeMs || item->endTimeMs <= timeMs)
            continue;

        BookmarkItemList bookmarkItems = d->bookmarksForItem(item);
        for (const BookmarkItemPtr &bookmarkItem: bookmarkItems) {
            Q_ASSERT_X(bookmarkItem->bookmark, Q_FUNC_INFO, "Zero level item should contain real bookmarks");
            if (bookmarkItem->bookmark)
                result.append(bookmarkItem->bookmark.get());
        }

    }

    return result;
}

void QnBookmarkMergeHelper::setBookmarks(const QnCameraBookmarkList &bookmarks)
{
    Q_D(QnBookmarkMergeHelper);
    d->clear();

    DetailLevel &zeroLevel = d->detailLevels.first();
    for (const QnCameraBookmark &bookmark: bookmarks)
        zeroLevel.items.append(BookmarkItemPtr(new BookmarkItem(bookmark)));

    for (int i = 1; i < d->detailLevels.size(); ++i)
        d->mergeBookmarkItems(i);
}

void QnBookmarkMergeHelper::addBookmark(const QnCameraBookmark &bookmark)
{

}

void QnBookmarkMergeHelper::removeBookmark(const QnUuid &bookmarkId)
{

}


int QnBookmarkMergeHelperPrivate::detailLevel(qint64 msecsPerDPix) const
{
    for (int i = 1; i < detailLevels.size(); ++i) {
        if (detailLevels[i].msecsPerDpix > msecsPerDPix)
            return i - 1;
    }
    return detailLevels.size() - 1;
}

void QnBookmarkMergeHelperPrivate::clear()
{
    for (DetailLevel &detailLevel: detailLevels)
        detailLevel.items.clear();
}

void QnBookmarkMergeHelperPrivate::mergeBookmarkItems(int detailLevel)
{
    Q_ASSERT_X(detailLevel > 0, Q_FUNC_INFO, "Zero level should not be merged");
    if (detailLevel <= 0)
        return;

    Q_ASSERT_X(detailLevel < detailLevels.size(), Q_FUNC_INFO, "Invalid detail level");
    if (detailLevel >= detailLevels.size())
        return;

    DetailLevel &currentLevel = detailLevels[detailLevel];
    DetailLevel &prevLevel = detailLevels[detailLevel - 1];
    qint64 mergeDistance = bookmarkMergeDistanceDpix * currentLevel.msecsPerDpix;
    qint64 mergeSpacing = bookmarkMergeSpacingDpix * currentLevel.msecsPerDpix;

    BookmarkItemPtr currentItem;

    for (BookmarkItemPtr &item: prevLevel.items) {
        if (currentItem) {
            if (item->startTimeMs - currentItem->startTimeMs < mergeDistance &&
                item->startTimeMs - currentItem->endTimeMs < mergeSpacing)
            {
                currentItem->endTimeMs = qMax(currentItem->endTimeMs, item->endTimeMs);
                currentItem->bookmark.reset();
                item->parent = currentItem;
            } else {
                currentLevel.items.append(currentItem);
                currentItem.clear();
            }
        }

        if (!currentItem) {
            if (item->bookmark) {
                currentItem = BookmarkItemPtr(new BookmarkItem(item->bookmark.get()));
            } else {
                currentItem = BookmarkItemPtr(new BookmarkItem());
                currentItem->startTimeMs = item->startTimeMs;
                currentItem->endTimeMs = item->endTimeMs;
            }
            item->parent = currentItem;
        }
    }

    if (currentItem)
        currentLevel.items.append(currentItem);
}

void QnBookmarkMergeHelperPrivate::adjustItemBoundsAfterRemoval(BookmarkItemPtr item, int level, const BookmarkItemPtr &removedItem)
{
    bool adjustStart = (removedItem->startTimeMs == item->startTimeMs);
    bool adjustEnd = (removedItem->endTimeMs == item->endTimeMs);

    if (!adjustStart && !adjustEnd)
        return;

    const DetailLevel &currentLevel = detailLevels[level];
    auto itemIt = std::find(currentLevel.items.begin(), currentLevel.items.end(), item);

    Q_ASSERT_X(itemIt != currentLevel.items.end(), Q_FUNC_INFO, "Item should be present in the specified detail level.");
    if (itemIt == currentLevel.items.end())
        return;

    const DetailLevel &zeroLevel = detailLevels.first();

    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), removedItem,
                               [](const BookmarkItemPtr &left, const BookmarkItemPtr &right)
    {
        return left->startTimeMs < right->startTimeMs;
    });

    Q_ASSERT_X(it != zeroLevel.items.end(), Q_FUNC_INFO, "Attempt to adjust an empty cluster");
    if (it == zeroLevel.items.end())
        return;

    if (adjustStart)
        item->startTimeMs = (*it)->startTimeMs;

    if (adjustEnd) {
        auto nextItemIt = itemIt + 1;

        qint64 searchEndTimeMs = (nextItemIt == currentLevel.items.end())
                                 ? DATETIME_NOW
                                 : (*nextItemIt)->startTimeMs;

        item->endTimeMs = item->startTimeMs;
        while ((*it)->startTimeMs < searchEndTimeMs) {
            item->endTimeMs = qMax(item->endTimeMs, (*it)->endTimeMs);
            ++it;
        }
    }
}

BookmarkItemList QnBookmarkMergeHelperPrivate::bookmarksForItem(const BookmarkItemPtr &item) const
{
    if (item->bookmark) {
        // We can return item from the highest levels if it is a standalone bookmark
        return BookmarkItemList() << item;
    }

    const DetailLevel &zeroLevel = detailLevels.first();

    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), item->startTimeMs,
                               [](const BookmarkItemPtr &item, qint64 startTimeMs)
    {
        return item->startTimeMs < startTimeMs;
    });

    BookmarkItemList result;

    while (it != zeroLevel.items.end() && (*it)->startTimeMs < item->endTimeMs) {
        if ((*it)->isParentedBy(item))
            result.append(*it);
        ++it;
    }

    return result;
}


QnTimelineBookmarkItem BookmarkItem::toTimelineBookmarkItem() const
{
    if (bookmark)
        return bookmark.get();

    QnBookmarkCluster cluster;
    cluster.startTimeMs = startTimeMs;
    cluster.durationMs = endTimeMs - startTimeMs;

    return cluster;
}

bool BookmarkItem::isParentedBy(const BookmarkItemPtr &parent) const
{
    return !this->parent.isNull() && (this->parent == parent || this->parent->isParentedBy(parent));
}
