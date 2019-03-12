#include "bookmark_merge_helper.h"

#include <QtCore/QSharedPointer>

#include <boost/optional/optional.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <core/resource/camera_bookmark.h>

#include <nx/utils/datetime.h>
#include <nx/utils/log/assert.h>

using std::chrono::milliseconds;
using namespace std::literals::chrono_literals;

namespace {

struct BookmarkItem;
typedef QSharedPointer<BookmarkItem> BookmarkItemPtr;
typedef QList<BookmarkItemPtr> BookmarkItemList;

constexpr int kExpandAreaDpix = 20;
constexpr int kDefaultMergeDistanseDpix = 100;
constexpr int kDefaultMergeSpacingDpix = 20;

struct BookmarkItem
{
    boost::optional<QnCameraBookmark> bookmark;

    BookmarkItemPtr parent;

    milliseconds startTimeMs;
    milliseconds endTimeMs;

public:
    BookmarkItem():
        startTimeMs(0),
        endTimeMs(0)
    {
    }

    BookmarkItem(const QnCameraBookmark &bookmark):
        bookmark(bookmark),
        startTimeMs(bookmark.startTimeMs),
        endTimeMs(bookmark.endTime())
    {
    }

    BookmarkItem(const BookmarkItem &item):
        bookmark(item.bookmark),
        startTimeMs(item.startTimeMs),
        endTimeMs(item.endTimeMs)
    {
    }

    QnTimelineBookmarkItem toTimelineBookmarkItem() const;
    bool isParentedBy(const BookmarkItemPtr &parent) const;
};

struct DetailLevel
{
    milliseconds msecsPerDpix;
    milliseconds mergeDistance;
    milliseconds mergeSpacing;
    QList<BookmarkItemPtr> items;

    DetailLevel(
        milliseconds  msecsInDpix,
        qint64 mergeDistanceDpix = kDefaultMergeDistanseDpix,
        qint64 mergeSpacingDpix = kDefaultMergeSpacingDpix)
        :
        msecsPerDpix(msecsInDpix),
        mergeDistance(mergeDistanceDpix * msecsInDpix),
        mergeSpacing(mergeSpacingDpix * msecsPerDpix)
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
    int bookmarkMergeDistanceDpix = 100;
    int bookmarkMergeSpacingDpix = 20;
    QList<DetailLevel> detailLevels;

public:
    int detailLevel(milliseconds msecsPerDPix) const;
    void clear();
    void mergeBookmarkItems(int detailLevel);
    void adjustItemBoundsAfterRemoval(BookmarkItemPtr item, int level, const BookmarkItemPtr &removedItem);
    BookmarkItemList bookmarksForItem(const BookmarkItemPtr &item) const;
    BookmarkItemList childrenForItem(const BookmarkItemPtr &item, int itemLevel) const;
};

QnBookmarkMergeHelper::QnBookmarkMergeHelper():
    d_ptr(new QnBookmarkMergeHelperPrivate)
{
    Q_D(QnBookmarkMergeHelper);
    d->detailLevels << DetailLevel(1ms)   // Zero level - not merged
                    << DetailLevel(250ms)
                    << DetailLevel(500ms)
                    << DetailLevel(1s)
                    << DetailLevel(2500ms)
                    << DetailLevel(5s)
                    << DetailLevel(15s)
                    << DetailLevel(30s)
                    << DetailLevel(1min)
                    << DetailLevel(5min)
                    << DetailLevel(15min)
                    << DetailLevel(30min)
                    << DetailLevel(1h)
                    << DetailLevel(3h)
                    << DetailLevel(6h)
                    << DetailLevel(12h)
                    << DetailLevel(24h) // Days are only in C++20
                    << DetailLevel(5 * 24h) // 5 days
                    << DetailLevel(15 * 24h)
                    << DetailLevel(30 * 24h) // 1 month
                    << DetailLevel(3 * 30 * 24h);   // 3 months
}

QnBookmarkMergeHelper::~QnBookmarkMergeHelper()
{
}

QnTimelineBookmarkItemList QnBookmarkMergeHelper::bookmarks(milliseconds msecsPerDp) const
{
    Q_D(const QnBookmarkMergeHelper);

    QnTimelineBookmarkItemList result;
    for (const BookmarkItemPtr &item: d->detailLevels[d->detailLevel(msecsPerDp)].items)
        result.append(item->toTimelineBookmarkItem());

    return result;
}

int QnBookmarkMergeHelper::indexAtPosition(const QnTimelineBookmarkItemList& bookmarks,
    milliseconds time, milliseconds msecsPerDp, BookmarkSearchOptions options)
{
    // Calculate distance to the center of a bookmark to reach total active area of 20 px.
    auto bookmarkDistance =
        [&bookmarks, time](int index)
        {
            const QnTimelineBookmarkItem& bookmarkItem = bookmarks[index];
            return std::abs((bookmarkItem.startTime() + bookmarkItem.endTime()).count()
                / 2 - time.count());
        };

    int foundItemIndex = -1; //< There is an item on the given position.
    int nearestItemIndex = -1;

    // We are not interested in bookmarks that are too far away.
    int nearestBookmarkDistance = kExpandAreaDpix * msecsPerDp.count() / 2;

    for (int i = 0; i < bookmarks.size(); ++i)
    {
        const QnTimelineBookmarkItem& bookmarkItem = bookmarks[i];
        if (time >= bookmarkItem.startTime() && time <= bookmarkItem.endTime())
        {
            foundItemIndex = i;

            // The topmost is the latest
            if (!options.testFlag(OnlyTopmost))
                return i;
        }

        if (foundItemIndex < 0 && options.testFlag(ExpandArea))
        {
            const auto distance = bookmarkDistance(i);
            if (distance < nearestBookmarkDistance)
            {
                nearestItemIndex = i;
                nearestBookmarkDistance = distance;
            }
        }
    }

    return foundItemIndex < 0 ? nearestItemIndex : foundItemIndex;
}

QnCameraBookmarkList QnBookmarkMergeHelper::bookmarksAtPosition(milliseconds time,
    milliseconds msecsPerDp, BookmarkSearchOptions options) const
{
    const bool onlyTopmost = options.testFlag(OnlyTopmost);

    Q_D(const QnBookmarkMergeHelper);

    const DetailLevel &level = d->detailLevels[d->detailLevel(msecsPerDp)];

    QnCameraBookmarkList result;

    if (onlyTopmost)
    {
        for (const auto& item: level.items | boost::adaptors::reversed)
        {
            if (time >= item->startTimeMs && time <= item->endTimeMs)
            {
                for (const auto& bookmarkItem: d->bookmarksForItem(item))
                {
                    NX_ASSERT(bookmarkItem->bookmark,
                        "Zero level item should contain real bookmarks");
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
        for (const auto& item: level.items)
        {
            if (item->startTimeMs > time || item->endTimeMs <= time)
                continue;

            BookmarkItemList bookmarkItems = d->bookmarksForItem(item);
            for (const BookmarkItemPtr& bookmarkItem : bookmarkItems)
            {
                NX_ASSERT(bookmarkItem->bookmark, "Zero level item should contain real bookmarks");
                if (bookmarkItem->bookmark)
                    result.append(bookmarkItem->bookmark.get());
            }
        }
    }

    if (result.empty() && options.testFlag(ExpandArea))
    {
        const auto items = bookmarks(msecsPerDp);
        NX_ASSERT(items.size() == level.items.size());
        int nearestItemIndex = indexAtPosition(items, time, msecsPerDp, options);
        if (nearestItemIndex >= 0)
        {
            auto item = level.items[nearestItemIndex];
            for (const auto& bookmarkItem: d->bookmarksForItem(item))
            {
                NX_ASSERT(bookmarkItem->bookmark,
                    "Zero level item should contain real bookmarks");
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

    DetailLevel& zeroLevel = d->detailLevels.first();
    auto itr = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), item, &bookmarkItemLess);
    zeroLevel.items.insert(itr, item);

    for (int i = 1; i < d->detailLevels.size(); ++i)
    {
        DetailLevel &currentLevel = d->detailLevels[i];

        const auto it = std::lower_bound(currentLevel.items.begin(), currentLevel.items.end(), item, &bookmarkItemLess);

        if (it != currentLevel.items.begin() && currentLevel.shouldBeMerged(*(it - 1), item))
        {
            BookmarkItemPtr existingItem = *(it - 1);
            item->parent = existingItem;
            existingItem->bookmark.reset();

            while (existingItem)
            {
                if (item->endTimeMs <= existingItem->endTimeMs)
                    break;

                existingItem->endTimeMs = item->endTimeMs;
                existingItem = existingItem->parent;
            }
        }
        else if (it != currentLevel.items.end() && currentLevel.shouldBeMerged(item, *it))
        {
            BookmarkItemPtr existingItem = *it;
            item->parent = existingItem;
            existingItem->bookmark.reset();

            while (existingItem)
            {
                if (item->startTimeMs >= existingItem->startTimeMs)
                    break;

                existingItem->startTimeMs = item->startTimeMs;
                existingItem = existingItem->parent;
            }
        }
        else
        {
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

    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(),
        BookmarkItemPtr(new BookmarkItem(bookmark)), &bookmarkItemLess);
    if (it == zeroLevel.items.end() || (*it)->bookmark.get().guid != bookmark.guid)
        return;

    BookmarkItemPtr item = *it;
    zeroLevel.items.erase(it);

    BookmarkItemPtr parent = item->parent;
    int level = 1;
    while (parent)
    {
        DetailLevel &currentLevel = d->detailLevels[level];

        if (parent->bookmark)
        {
            auto parentIt = std::lower_bound(currentLevel.items.begin(), currentLevel.items.end(), parent, &bookmarkItemLess);
            if (parentIt == zeroLevel.items.end() || (*parentIt)->bookmark.get().guid != bookmark.guid)
            {
                NX_ASSERT(false, "Bookmark item parent is not found in a higher level.");
                return;
            }
            currentLevel.items.erase(parentIt);
        }
        else
        {
            if (item->startTimeMs == parent->startTimeMs || item->endTimeMs == parent->endTimeMs)
            {
                const auto children = d->childrenForItem(parent, level);
                if (children.isEmpty())
                {
                    NX_ASSERT(false, "Bookmark children list should not be empty at the moment.");
                    return;
                }
                parent->startTimeMs = children.first()->startTimeMs;
                parent->endTimeMs = parent->startTimeMs;
                for (const BookmarkItemPtr &child: children)
                    parent->endTimeMs = qMax(parent->endTimeMs, child->endTimeMs);

                if (children.size() == 1)
                    parent->bookmark = children.first()->bookmark;
            }
            else
                break;
        }

        parent = parent->parent;
    }
}


int QnBookmarkMergeHelperPrivate::detailLevel(milliseconds msecsPerDPix) const
{
    for (int i = 1; i < detailLevels.size(); ++i)
        if (detailLevels[i].msecsPerDpix > msecsPerDPix)
            return i - 1;

    return detailLevels.size() - 1;
}

void QnBookmarkMergeHelperPrivate::clear()
{
    for (DetailLevel &detailLevel: detailLevels)
        detailLevel.items.clear();
}

void QnBookmarkMergeHelperPrivate::mergeBookmarkItems(int detailLevel)
{
    NX_ASSERT(detailLevel > 0, "Zero level should not be merged");
    if (detailLevel <= 0)
        return;

    NX_ASSERT(detailLevel < detailLevels.size(), "Invalid detail level");
    if (detailLevel >= detailLevels.size())
        return;

    DetailLevel &currentLevel = detailLevels[detailLevel];
    DetailLevel &prevLevel = detailLevels[detailLevel - 1];

    BookmarkItemPtr currentItem;

    for (BookmarkItemPtr &item: prevLevel.items)
    {
        if (currentItem)
        {
            if (currentLevel.shouldBeMerged(currentItem, item))
            {
                currentItem->endTimeMs = qMax(currentItem->endTimeMs, item->endTimeMs);
                currentItem->bookmark.reset();
                item->parent = currentItem;
            }
            else
            {
                currentLevel.items.append(currentItem);
                currentItem.clear();
            }
        }

        if (!currentItem)
        {
            if (item->bookmark)
            {
                currentItem = BookmarkItemPtr(new BookmarkItem(item->bookmark.get()));
            }
            else
            {
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

    NX_ASSERT(itemIt != currentLevel.items.end(), "Item should be present in the specified detail level.");
    if (itemIt == currentLevel.items.end())
        return;

    const DetailLevel &zeroLevel = detailLevels.first();

    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), removedItem, &bookmarkItemLess);

    NX_ASSERT(it != zeroLevel.items.end(), "Attempt to adjust an empty cluster");
    if (it == zeroLevel.items.end())
        return;

    if (adjustStart)
        item->startTimeMs = (*it)->startTimeMs;

    if (adjustEnd)
    {
        auto nextItemIt = itemIt + 1;

        milliseconds searchEndTimeMs = (nextItemIt == currentLevel.items.end())
                                 ? milliseconds(DATETIME_NOW)
                                 : (*nextItemIt)->startTimeMs;

        item->endTimeMs = item->startTimeMs;
        while ((*it)->startTimeMs < searchEndTimeMs)
        {
            item->endTimeMs = qMax(item->endTimeMs, (*it)->endTimeMs);
            ++it;
        }
    }
}

BookmarkItemList QnBookmarkMergeHelperPrivate::bookmarksForItem(const BookmarkItemPtr &item) const
{
    if (item->bookmark)
    {
        // We can return item from the highest levels if it is a standalone bookmark
        return BookmarkItemList() << item;
    }

    const DetailLevel &zeroLevel = detailLevels.first();

    auto it = std::lower_bound(zeroLevel.items.begin(), zeroLevel.items.end(), item->startTimeMs,
        [](const BookmarkItemPtr &item, milliseconds startTimeMs)
        {
            return item->startTimeMs < startTimeMs;
        });

    BookmarkItemList result;

    while (it != zeroLevel.items.end() && (*it)->startTimeMs < item->endTimeMs)
    {
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
        [](const BookmarkItemPtr &item, milliseconds startTimeMs)
        {
            return item->startTimeMs < startTimeMs;
        });

    BookmarkItemList result;

    while (it != prevLevel.items.end() && (*it)->startTimeMs < item->endTimeMs)
    {
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
    cluster.startTime = startTimeMs;
    cluster.duration = endTimeMs - startTimeMs;

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

    return right->startTimeMs - left->startTimeMs < mergeDistance
        && right->startTimeMs - left->endTimeMs < mergeSpacing;
}
