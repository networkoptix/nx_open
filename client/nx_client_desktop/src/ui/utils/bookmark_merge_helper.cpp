#include "bookmark_merge_helper.h"

#include <boost/optional/optional.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <core/resource/camera_bookmark.h>

#include <nx/utils/datetime.h>

namespace {

struct BookmarkItem;
typedef QSharedPointer<BookmarkItem> BookmarkItemPtr;
typedef QList<BookmarkItemPtr> BookmarkItemList;

enum {
    kDefaultMergeDistanseDpix = 100,
    kDefaultMergeSpacingDpix = 20
};

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

    BookmarkItem(const BookmarkItem &item)
        : bookmark(item.bookmark)
        , startTimeMs(item.startTimeMs)
        , endTimeMs(item.endTimeMs)
    {
    }

    QnTimelineBookmarkItem toTimelineBookmarkItem() const;
    bool isParentedBy(const BookmarkItemPtr &parent) const;
};

struct DetailLevel
{
    qint64 msecsPerDpix;
    qint64 mergeDistance;
    qint64 mergeSpacing;
    QList<BookmarkItemPtr> items;

    DetailLevel(
            qint64 msecsInDpix,
            qint64 mergeDistanceDpix = kDefaultMergeDistanseDpix,
            qint64 mergeSpacingDpix = kDefaultMergeSpacingDpix)
        : msecsPerDpix(msecsInDpix)
        , mergeDistance(mergeDistanceDpix * msecsInDpix)
        , mergeSpacing(mergeSpacingDpix * msecsPerDpix)
    {
    }

    bool shouldBeMerged(const BookmarkItemPtr &left, const BookmarkItemPtr &right) const;
};

bool bookmarkItemLess(const BookmarkItemPtr &left, const BookmarkItemPtr &right)
{
    if (left->startTimeMs != right->startTimeMs)
        return left->startTimeMs < right->startTimeMs;

    if (left->bookmark && right->bookmark)
        return left->bookmark.get().guid < right->bookmark.get().guid;

    return false;
}

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
    BookmarkItemList childrenForItem(const BookmarkItemPtr &item, int itemLevel) const;
};

QnBookmarkMergeHelper::QnBookmarkMergeHelper() :
    d_ptr(new QnBookmarkMergeHelperPrivate)
{
    Q_D(QnBookmarkMergeHelper);
    d->detailLevels << DetailLevel(0)   // Zero level - not merged
                    << DetailLevel(250)     // 250 ms
                    << DetailLevel(500)     // 500 ms
                    << DetailLevel(1000)        // 1 sec
                    << DetailLevel(2500)        // 2.5 sec
                    << DetailLevel(5 * 1000ll)    // 5 sec
                    << DetailLevel(15 * 1000ll)   // 15 sec
                    << DetailLevel(30 * 1000ll)   // 30 sec
                    << DetailLevel(60 * 1000ll)       // 1 min
                    << DetailLevel(5 * 60 * 1000ll)   // 5 min
                    << DetailLevel(15 * 60 * 1000ll)  // 15 min
                    << DetailLevel(30 * 60 * 1000ll)  // 30 min
                    << DetailLevel(60 * 60 * 1000ll)          // 1 hr
                    << DetailLevel(3 * 60 * 60 * 1000ll)      // 3 hr
                    << DetailLevel(6 * 60 * 60 * 1000ll)      // 6 hr
                    << DetailLevel(12 * 60 * 60 * 1000ll)     // 12 hr
                    << DetailLevel(24 * 60 * 60 * 1000ll)         // 1 day
                    << DetailLevel(5 * 24 * 60 * 60 * 1000ll)     // 5 days
                    << DetailLevel(15 * 24 * 60 * 60 * 1000ll)    // 15 days
                    << DetailLevel(30 * 24 * 60 * 60 * 1000ll)        // 1 month
                    << DetailLevel(3 * 30 * 24 * 60 * 60 * 1000ll);   // 3 months
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

QnCameraBookmarkList QnBookmarkMergeHelper::bookmarksAtPosition(qint64 timeMs, int msecsPerDp, bool onlyTopmost) const
{
    Q_D(const QnBookmarkMergeHelper);

    const DetailLevel &level = d->detailLevels[d->detailLevel(msecsPerDp)];

    QnCameraBookmarkList result;

    if (onlyTopmost)
    {
        for (const BookmarkItemPtr& item : level.items | boost::adaptors::reversed)
        {
            if (timeMs >= item->startTimeMs && timeMs <= item->endTimeMs)
            {
                for (const BookmarkItemPtr& bookmarkItem : d->bookmarksForItem(item))
                {
                    NX_ASSERT(bookmarkItem->bookmark, Q_FUNC_INFO, "Zero level item should contain real bookmarks");
                    if (bookmarkItem->bookmark)
                        result.append(bookmarkItem->bookmark.get());
                }

                if (!result.isEmpty())
                    break;
            }
        }
    }
    else
    {
        for (const BookmarkItemPtr& item : level.items)
        {
            if (item->startTimeMs > timeMs || item->endTimeMs <= timeMs)
                continue;

            BookmarkItemList bookmarkItems = d->bookmarksForItem(item);
            for (const BookmarkItemPtr& bookmarkItem : bookmarkItems)
            {
                NX_ASSERT(bookmarkItem->bookmark, Q_FUNC_INFO, "Zero level item should contain real bookmarks");
                if (bookmarkItem->bookmark)
                    result.append(bookmarkItem->bookmark.get());
            }
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
    /* Here we perform a simplified bookmark merge.
     * In contradiction to the main algorithm this one won't change the bookmarks tree topology.
     * E.g. it won't merge existing clusters that weren't merged before beacuse of big spacing between them
     * which became filled by the added bookmark.
     * Instead such bookmark will be appended to the left or right cluster if possible.
     */

    Q_D(QnBookmarkMergeHelper);

    BookmarkItemPtr item(new BookmarkItem(bookmark));

    DetailLevel &zeroLevel = d->detailLevels.first();
    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), item, &bookmarkItemLess);
    zeroLevel.items.insert(it, item);

    for (int i = 1; i < d->detailLevels.size(); ++i) {
        DetailLevel &currentLevel = d->detailLevels[i];

        const auto it = std::lower_bound(currentLevel.items.begin(), currentLevel.items.end(), item, &bookmarkItemLess);

        if (it != currentLevel.items.begin() && currentLevel.shouldBeMerged(*(it - 1), item)) {
            BookmarkItemPtr existingItem = *(it - 1);
            item->parent = existingItem;
            existingItem->bookmark.reset();

            while (existingItem) {
                if (item->endTimeMs <= existingItem->endTimeMs)
                    break;

                existingItem->endTimeMs = item->endTimeMs;
                existingItem = existingItem->parent;
            }
        } else if (it != currentLevel.items.end() && currentLevel.shouldBeMerged(item, *it)) {
            BookmarkItemPtr existingItem = *it;
            item->parent = existingItem;
            existingItem->bookmark.reset();

            while (existingItem) {
                if (item->startTimeMs >= existingItem->startTimeMs)
                    break;

                existingItem->startTimeMs = item->startTimeMs;
                existingItem = existingItem->parent;
            }
        } else {
            BookmarkItemPtr newItem(new BookmarkItem(*item.data()));
            currentLevel.items.insert(it, newItem);
            item->parent = newItem;
            item = newItem;
        }
    }
}

void QnBookmarkMergeHelper::removeBookmark(const QnCameraBookmark &bookmark)
{
    /* Here we perform simple bookmark removal.
     * In contradiction to the main merge algorithm here we don't change the bookmarks tree topology.
     * E.g. we don't split existing clusters that should be split beacuse of big spacing between their child items.
     * Instead we only adjust its bounds.
     */

    Q_D(QnBookmarkMergeHelper);

    DetailLevel &zeroLevel = d->detailLevels.first();

    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), BookmarkItemPtr(new BookmarkItem(bookmark)), &bookmarkItemLess);
    if (it == zeroLevel.items.end() || (*it)->bookmark.get().guid != bookmark.guid)
        return;

    BookmarkItemPtr item = *it;
    zeroLevel.items.erase(it);

    BookmarkItemPtr parent = item->parent;
    int level = 1;
    while (parent) {
        DetailLevel &currentLevel = d->detailLevels[level];

        if (parent->bookmark) {
            auto parentIt = std::lower_bound(currentLevel.items.begin(), currentLevel.items.end(), parent, &bookmarkItemLess);
            if (parentIt == zeroLevel.items.end() || (*parentIt)->bookmark.get().guid != bookmark.guid) {
                NX_ASSERT(false, Q_FUNC_INFO, "Bookmark item parent is not found in a higher level.");
                return;
            }
            currentLevel.items.erase(parentIt);
        } else {
            if (item->startTimeMs == parent->startTimeMs || item->endTimeMs == parent->endTimeMs) {
                const auto children = d->childrenForItem(parent, level);
                if (children.isEmpty()) {
                    NX_ASSERT(false, Q_FUNC_INFO, "Bookmark children list should not be empty at the moment.");
                    return;
                }
                parent->startTimeMs = children.first()->startTimeMs;
                parent->endTimeMs = parent->startTimeMs;
                for (const BookmarkItemPtr &child: children)
                    parent->endTimeMs = qMax(parent->endTimeMs, child->endTimeMs);

                if (children.size() == 1)
                    parent->bookmark = children.first()->bookmark;
            } else {
                break;
            }
        }

        parent = parent->parent;
    }
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
    NX_ASSERT(detailLevel > 0, Q_FUNC_INFO, "Zero level should not be merged");
    if (detailLevel <= 0)
        return;

    NX_ASSERT(detailLevel < detailLevels.size(), Q_FUNC_INFO, "Invalid detail level");
    if (detailLevel >= detailLevels.size())
        return;

    DetailLevel &currentLevel = detailLevels[detailLevel];
    DetailLevel &prevLevel = detailLevels[detailLevel - 1];

    BookmarkItemPtr currentItem;

    for (BookmarkItemPtr &item: prevLevel.items) {
        if (currentItem) {
            if (currentLevel.shouldBeMerged(currentItem, item)) {
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

    NX_ASSERT(itemIt != currentLevel.items.end(), Q_FUNC_INFO, "Item should be present in the specified detail level.");
    if (itemIt == currentLevel.items.end())
        return;

    const DetailLevel &zeroLevel = detailLevels.first();

    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), removedItem, &bookmarkItemLess);

    NX_ASSERT(it != zeroLevel.items.end(), Q_FUNC_INFO, "Attempt to adjust an empty cluster");
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

BookmarkItemList QnBookmarkMergeHelperPrivate::childrenForItem(const BookmarkItemPtr &item, int itemLevel) const
{
    if (itemLevel == 0)
        return BookmarkItemList();

    const DetailLevel &prevLevel = detailLevels[itemLevel - 1];

    auto it = std::lower_bound(prevLevel.items.begin(), prevLevel.items.end(), item->startTimeMs,
                               [](const BookmarkItemPtr &item, qint64 startTimeMs)
    {
        return item->startTimeMs < startTimeMs;
    });

    BookmarkItemList result;

    while (it != prevLevel.items.end() && (*it)->startTimeMs < item->endTimeMs) {
        if ((*it)->parent == item)
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

bool DetailLevel::shouldBeMerged(const BookmarkItemPtr &left, const BookmarkItemPtr &right) const
{
    if (left->startTimeMs > right->startTimeMs)
        return shouldBeMerged(right, left);

    return right->startTimeMs - left->startTimeMs < mergeDistance &&
           right->startTimeMs - left->endTimeMs < mergeSpacing;
}
