// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmark.h"

#include <QtCore/QLinkedList>

#include <nx/fusion/model_functions.h>
#include <nx/vms/common/resource/bookmark_filter.h>
#include <nx/vms/common/system_context.h>
#include <utils/camera/bookmark_helpers.h>
#include <utils/camera/camera_names_watcher.h>

using namespace std::chrono;
using namespace nx::vms::common;

namespace
{
// TODO: #ynikitenkov make generic version of algorithm
typedef std::function<bool (const QnCameraBookmark &first, const QnCameraBookmark &second)> BinaryPredicate;
QnCameraBookmarkList mergeSortedBookmarks(
    const QnMultiServerCameraBookmarkList &source,
    const BinaryPredicate &pred,
    int limit)
{
    int totalSize = 0;

    typedef std::vector<const QnCameraBookmarkList *> NonEmptyBookmarksList;
    const auto nonEmptySources = [&totalSize, &source]() -> NonEmptyBookmarksList
    {
        NonEmptyBookmarksList result;
        for (const auto &bookmarks: source)
        {
            if (bookmarks.empty())
                continue;

            totalSize += bookmarks.size();
            result.push_back(&bookmarks);
        }
        return result;
    }();

    if (nonEmptySources.empty())
        return QnCameraBookmarkList();

    if (nonEmptySources.size() == 1)
    {
        const QnCameraBookmarkList &result = *nonEmptySources.front();
        return result.size() <= limit ? result : result.mid(0, limit);
    }
    else if (nonEmptySources.size() == 2)
    {
        QnCameraBookmarkList result;
        const auto& source = nonEmptySources;

        result.resize(source[0]->size() + source[1]->size());
        auto itr = std::set_union(
            source[0]->constBegin(), source[0]->constEnd(),
            source[1]->constBegin(), source[1]->constEnd(),
            result.begin(), pred);
        if (!result.empty())
            result.resize(itr - result.begin());
        return result.size() <= limit ? result : result.mid(0, limit);
    }

    typedef QPair<QnCameraBookmarkList::const_iterator,
        QnCameraBookmarkList::const_iterator> IteratorsPair;
    typedef std::vector<IteratorsPair> MergeDataVector;

    MergeDataVector mergeData =
        [nonEmptySources]() -> MergeDataVector
        {
            MergeDataVector result;
            for (const auto source: nonEmptySources)
                result.push_back(IteratorsPair(source->begin(), source->end()));
            return result;
        }();

    const int resultSize = std::min(totalSize, limit);

    QnCameraBookmarkList result;
    result.reserve(resultSize);
    while(!mergeData.empty() && (result.size() < resultSize))
    {
        // Looking for bookmarks list with minimal value
        auto mergeDataIt = mergeData.begin();
        const QnCameraBookmark *minBookmark = mergeDataIt->first;
        for (auto it = mergeDataIt + 1; it != mergeData.end(); ++it)
        {
            const QnCameraBookmark *currentBookmark= it->first;
            if (!pred(*currentBookmark, *minBookmark))
                continue;

            mergeDataIt = it;
            minBookmark = currentBookmark;
        }

        result.append(*minBookmark);
        if (++mergeDataIt->first == mergeDataIt->second)    // if list with min element is logically empty
            mergeData.erase(mergeDataIt);

        // TODO: #ynikitenkov add copy elements if it is only one list
    }
    return result;
}

template<typename GetterType>
BinaryPredicate makePredByGetter(const GetterType &getter, bool isAscending)
{
    const BinaryPredicate ascPred =
        [getter](const QnCameraBookmark &first, const QnCameraBookmark &second)
        {
            const auto firstValue = getter(first);
            const auto secondValue = getter(second);
            if (firstValue != secondValue)
                return firstValue < secondValue;
            return first.guid < second.guid;
        };

    const BinaryPredicate descPred =
        [getter](const QnCameraBookmark &first, const QnCameraBookmark &second)
        {
            const auto firstValue = getter(first);
            const auto secondValue = getter(second);
            if (firstValue != secondValue)
                return firstValue > secondValue;
            return first.guid > second.guid;
        };

    return isAscending ? ascPred : descPred;
}

BinaryPredicate createPredicate(SystemContext* systemContext, const QnBookmarkSortOrder &sortOrder)
{
    const bool isAscending = (sortOrder.order == Qt::AscendingOrder);

    switch(sortOrder.column)
    {
        case nx::vms::api::BookmarkSortField::name:
        {
            return makePredByGetter(
                [](const QnCameraBookmark &bookmark)
                {
                    return bookmark.name;
                }, isAscending);
        }
        case nx::vms::api::BookmarkSortField::startTime:
        {
            return makePredByGetter(
                [](const QnCameraBookmark &bookmark)
                {
                    return bookmark.startTimeMs;
                }, isAscending);
        }
        case nx::vms::api::BookmarkSortField::duration:
        {
            return makePredByGetter(
                [](const QnCameraBookmark &bookmark)
                {
                    return bookmark.durationMs;
                }, isAscending);
        }
        case nx::vms::api::BookmarkSortField::creationTime:
        {
            return makePredByGetter(
                [](const QnCameraBookmark &bookmark)
                {
                    return bookmark.creationTime();
                }, isAscending);
        }
        case nx::vms::api::BookmarkSortField::tags:
        {
            static const auto tagsGetter =
                [](const QnCameraBookmark &bookmark)
                {
                    return QnCameraBookmark::tagsToString(bookmark.tags);
                };
            return makePredByGetter(tagsGetter, isAscending);
        }
        case nx::vms::api::BookmarkSortField::description:
        {
            static const auto descriptionGetter =
                [](const QnCameraBookmark &bookmark)
                {
                    return bookmark.description;
                };
            return makePredByGetter(descriptionGetter, isAscending);
        }
        case nx::vms::api::BookmarkSortField::creator:
        {
            const auto creatorGetter =
                [systemContext](const QnCameraBookmark& bookmark)
                {
                    auto resourcePool = systemContext->resourcePool();
                    return helpers::getBookmarkCreatorName(bookmark.creatorId, resourcePool);
                };
            return makePredByGetter(creatorGetter, isAscending);
        }
        case nx::vms::api::BookmarkSortField::cameraName:
        {
            const auto cameraNameGetter =
                [systemContext](const QnCameraBookmark &bookmark)
                {
                    return systemContext->cameraNamesWatcher()->getCameraName(bookmark.cameraId);
                };
            return makePredByGetter(cameraNameGetter, isAscending);
        }
        default:
        {
            NX_ASSERT(false, "Invalid bookmark sorting field!");
            return BinaryPredicate();
        }
    };
};

QnMultiServerCameraBookmarkList sortEachList(
    SystemContext* systemContext,
    QnMultiServerCameraBookmarkList sources,
    const QnBookmarkSortOrder &sortProp)
{
    for (auto &source: sources)
        QnCameraBookmark::sortBookmarks(systemContext, source, sortProp);

    return std::move(sources);
}

typedef QLinkedList<QnCameraBookmarkList::const_iterator> ItersLinkedList;

QnCameraBookmarkList createListFromIters(const ItersLinkedList &iters)
{
    QnCameraBookmarkList result;
    result.reserve(iters.size());
    for (auto it: iters)
        result.push_back(*it);
    return result;
};

QnCameraBookmarkList getSparseByIters(ItersLinkedList &bookmarkIters, int limit)
{
    NX_ASSERT(limit > 0, "Limit should be greater than 0!");
    if (limit <= 0)
        return QnCameraBookmarkList();

    if (bookmarkIters.size() <= limit)
        return createListFromIters(bookmarkIters);

    // Thin out bookmarks with calculated step
    const int removeCount = (bookmarkIters.size() - limit);
    const double removeStep = static_cast<double>(removeCount) / static_cast<double>(bookmarkIters.size());

    double counter = 0.0;
    double prevCounter = 0.0;
    double removedCounter = 1.0;
    for (auto it = bookmarkIters.begin(); (it != bookmarkIters.end() && bookmarkIters.size() > limit);)
    {
        if (qFuzzyBetween(prevCounter, removedCounter, counter))
        {
            it = bookmarkIters.erase(it); // We have to remove item if next integer counter is reached
            removedCounter += 1.0;
        }
        else
            ++it;

        prevCounter = counter;
        counter += removeStep;
    }
    return createListFromIters(bookmarkIters);
}

QnCameraBookmarkList getSparseBookmarks(
    const QnCameraBookmarkList &bookmarks,
    const std::optional<milliseconds>& minVisibleLength,
    int limit,
    const BinaryPredicate &pred)
{
    NX_ASSERT(limit > 0, "Limit should be greater than 0!");
    if (limit <= 0)
        return QnCameraBookmarkList();

    if (!minVisibleLength || (bookmarks.size() <= limit))
        return bookmarks;

    ItersLinkedList valuableBookmarksIters;
    ItersLinkedList nonValuableBookmarksIters;
    for (auto it = bookmarks.begin(); it != bookmarks.end(); ++it)
    {
        if (it->durationMs >= *minVisibleLength)
            valuableBookmarksIters.push_back(it);
        else if (valuableBookmarksIters.size() < limit)
            nonValuableBookmarksIters.push_back(it);
    }

    const int valuableBookmarksCount = valuableBookmarksIters.size();
    if (valuableBookmarksCount > limit)
        return getSparseByIters(valuableBookmarksIters, limit);   // Thin out unnecessary valuable bookmarks

    // Adds bookmarks from non-valuables to complete limit
    const int nonValuableBookmarkToAddCount = (limit - valuableBookmarksCount);
    QnMultiServerCameraBookmarkList toMergeLists;
    toMergeLists.push_back(createListFromIters(valuableBookmarksIters));
    toMergeLists.push_back(getSparseByIters(nonValuableBookmarksIters, nonValuableBookmarkToAddCount));
    return mergeSortedBookmarks(toMergeLists, pred, limit);
}
} // namespace


QnBookmarkSortOrder::QnBookmarkSortOrder(nx::vms::api::BookmarkSortField column, Qt::SortOrder order):
    column(column),
    order(order)
{
}

const QnBookmarkSortOrder QnBookmarkSortOrder::defaultOrder = QnBookmarkSortOrder();

const QString QnCameraBookmark::kGuidParam("guid");
const QString QnCameraBookmark::kCreationStartTimeParam("creationStartTimeMs");
const QString QnCameraBookmark::kCreationEndTimeParam("creationEndTimeMs");


milliseconds QnCameraBookmark::endTime() const
{
    return startTimeMs + durationMs;
}

bool QnCameraBookmark::isNull() const
{
    return guid.isNull();
}

QString QnCameraBookmark::tagsToString(const QnCameraBookmarkTags &tags, const QString &delimiter)
{
    QStringList validTags;
    for (const QString &tag: tags)
    {
        QString trimmed = tag.trimmed();
        if (!trimmed.isEmpty())
            validTags << trimmed;
    }
    return validTags.join(delimiter);
}

// TODO: #sivanov Create unit tests.
void QnCameraBookmark::sortBookmarks(
    SystemContext* systemContext,
    QnCameraBookmarkList &bookmarks,
    const QnBookmarkSortOrder orderBy)
{
    /* For some reason clang fails to compile this if createPredicate is passed directly to std::sort.
       Using lambda for this works. */
    auto pred = createPredicate(systemContext, orderBy);
    std::sort(bookmarks.begin(), bookmarks.end(), [pred](const QnCameraBookmark &first, const QnCameraBookmark &second)
    {
        return pred(first, second);
    });
}

milliseconds QnCameraBookmark::creationTime() const
{
    return creatorId.isNull() ? startTimeMs : creationTimeStampMs;
}

bool QnCameraBookmark::isCreatedBySystem() const
{
    return creatorId == systemUserId();
}

QnUuid QnCameraBookmark::systemUserId()
{
    return QnUuid::fromStringSafe("{51723d00-51bd-4420-8116-75e5f85dfcf4}");
}

QnCameraBookmark::QnCameraBookmark():
    creatorId(systemUserId()),
    timeout(-1),
    startTimeMs(0),
    durationMs(0)
{
}

QnCameraBookmarkList QnCameraBookmark::mergeCameraBookmarks(
    SystemContext* systemContext,
    const QnMultiServerCameraBookmarkList &source,
    const QnBookmarkSortOrder &sortOrder,
    const std::optional<milliseconds>& minVisibleLength,
    int limit)
{
    NX_ASSERT(limit > 0, "Limit should be greater than 0!");
    if (limit <= 0)
        return QnCameraBookmarkList();

    const auto pred = createPredicate(systemContext, sortOrder);

    const int intermediateLimit = (minVisibleLength ? QnCameraBookmarkSearchFilter::kNoLimit : limit);
    QnCameraBookmarkList result;
    switch(sortOrder.column)
    {
        case nx::vms::api::BookmarkSortField::creationTime:
        case nx::vms::api::BookmarkSortField::creator:
        case nx::vms::api::BookmarkSortField::cameraName:
        case nx::vms::api::BookmarkSortField::tags:
        {
            const auto bookmarksList = sortEachList(systemContext, source, sortOrder);
            result = mergeSortedBookmarks(bookmarksList, pred, intermediateLimit);
            break;
        }
        default:
        {
            // All data by other columns is sorted by database.
            result = mergeSortedBookmarks(source, pred, intermediateLimit);
        }
    }

    if (!minVisibleLength)
        return result;

    return getSparseBookmarks(result, minVisibleLength, limit, pred);
}

QnCameraBookmarkTagList QnCameraBookmarkTag::mergeCameraBookmarkTags(const QnMultiServerCameraBookmarkTagList &source, int limit) {
    NX_ASSERT(limit > 0, "Limit must be correct");
    if (limit <= 0)
        return QnCameraBookmarkTagList();

    QnMultiServerCameraBookmarkTagList nonEmptyLists;
    for (const auto &list: source)
        if (!list.isEmpty())
            nonEmptyLists.push_back(list);

    if (nonEmptyLists.empty())
        return QnCameraBookmarkTagList();

    if(nonEmptyLists.size() == 1)
    {
        QnCameraBookmarkTagList result = nonEmptyLists.front();
        if (result.size() > limit)
            result.resize(limit);
        return result;
    }

    QnCameraBookmarkTagList result;
    int maxSize = 0;
    for (const auto &list: nonEmptyLists)
        maxSize += list.size();
    result.reserve(std::min(limit, maxSize));

    QMap<QString, int> mergedTags;
    for (const QnCameraBookmarkTagList &source: nonEmptyLists)
    {
        for (const auto &tag: source)
        {
            auto &currentCount = mergedTags[tag.name];
            currentCount += tag.count;
        }
    }

    QMap<int, QString> sortedTags;
    for (auto it = mergedTags.begin(); it != mergedTags.end(); ++it)
        sortedTags.insert(it.value(), it.key());

    for (auto it = sortedTags.begin(); it != sortedTags.end(); ++it) {
        if (result.size() >= limit)
            break;

        result.push_front(QnCameraBookmarkTag(it.value(), it.key()));
    }

    return result;
}


bool QnCameraBookmark::isValid() const {
    return !isNull()
        && !name.isEmpty()
        && !cameraId.isNull()
        && durationMs > 0ms;
}

void serialize(nx::reflect::json::SerializationContext* ctx, const QnCameraBookmarkTags& value)
{
    ctx->composer.startArray();
    for (const auto& it: value)
    {
        nx::reflect::BasicSerializer::serializeAdl(ctx, it);
    }
    ctx->composer.endArray();
}

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx, QnCameraBookmarkTags* data)
{
    using namespace nx::reflect::json_detail;
    *data = QnCameraBookmarkTags();
    if (!ctx.value.IsArray())
        return false;

    for (rapidjson::SizeType i = 0; i < ctx.value.Size(); ++i)
    {
        QString element;
        const auto deserializationResult =
            deserializeValue(DeserializationContext{ctx.value[i], ctx.flags}, &element);
        if (!deserializationResult.success)
        {
            return false;
        }
        data->insert(element);
    }
    return true;
}

bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other)
{
    if (first.startTimeMs == other.startTimeMs)
        return first.guid.toRfc4122() < other.guid.toRfc4122();
    return first.startTimeMs < other.startTimeMs;
}

bool operator<(milliseconds first, const QnCameraBookmark &other)
{
    return first < other.startTimeMs;
}

bool operator<(const QnCameraBookmark &first, milliseconds other)
{
    return first.startTimeMs < other;
}

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark)
{
    if (bookmark.durationMs > 0ms)
    {
        dbg.nospace()
            << "QnCameraBookmark("
            << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs.count()).toString(lit("dd hh:mm"))
            << " - "
            << QDateTime::fromMSecsSinceEpoch((bookmark.startTimeMs + bookmark.durationMs).count()).
                toString(lit("dd hh:mm"))
            << ')';
    }
    else
    {
        dbg.nospace()
            << "QnCameraBookmark INSTANT ("
            << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs.count()).toString(lit("dd hh:mm"))
            << ')';
    }
    dbg.space() << "timeout" << bookmark.timeout.count();
    dbg.space() << bookmark.name << bookmark.description;
    dbg.space() << QnCameraBookmark::tagsToString(bookmark.tags);
    return dbg.space();
}

QString QnCameraBookmarkSearchFilter::toString() const
{
    return QString::fromStdString(nx::reflect::json::serialize(*this));
}

bool QnCameraBookmarkSearchFilter::isValid() const
{
    if (limit <= 0)
        return false;

    if (startTimeMs != milliseconds::zero() && endTimeMs != milliseconds::zero())
        return startTimeMs <= endTimeMs;

     return true;
}

bool QnCameraBookmarkSearchFilter::checkBookmark(const QnCameraBookmark &bookmark) const
{
    if (startTimeMs != milliseconds::zero() && bookmark.endTime() < startTimeMs)
        return false;

    if (endTimeMs != milliseconds::zero() && bookmark.startTimeMs > endTimeMs)
        return false;

    return text.isEmpty() || BookmarkTextFilter(text).match(bookmark);
}

const int QnCameraBookmarkSearchFilter::kNoLimit = std::numeric_limits<int>::max();

QString bookmarkToString(const QnCameraBookmark& bookmark)
{
    return bookmark.name;
}

QVariantList bookmarkListToVariantList(const QnCameraBookmarkList& bookmarks)
{
    QVariantList result;
    for  (const auto& b : bookmarks)
        result << QVariant::fromValue(b);
    return result;
}

QnCameraBookmarkList variantListToBookmarkList(const QVariantList& list)
{
    QnCameraBookmarkList result;
    for  (const auto& v : list)
        result << v.value<QnCameraBookmark>();
    return result;
}

void serialize_field(const QnCameraBookmarkTags& /*value*/, QVariant* /*target*/) {return ;}
void deserialize_field(const QVariant& /*value*/, QnCameraBookmarkTags* /*target*/) {return ;}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBookmarkSortOrder, (json), QnBookmarkSortOrder_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraBookmarkSearchFilter, (json), QnCameraBookmarkSearchFilter_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnCameraBookmark, (sql_record)(json)(ubjson)(xml)(csv_record), QnCameraBookmark_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmarkTag,
    (sql_record)(json)(ubjson)(xml)(csv_record), QnCameraBookmarkTag_Fields)
