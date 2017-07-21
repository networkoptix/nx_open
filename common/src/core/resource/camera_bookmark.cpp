#include "camera_bookmark.h"

#include <QtCore/QMap>
#include <QtCore/QLinkedList>

#include <nx/fusion/model_functions.h>
#include <utils/camera/camera_names_watcher.h>
#include <utils/camera/bookmark_helpers.h>
#include <common/common_module.h>

namespace
{
    //TODO: #ynikitenkov make generic version of algorithm
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
            if (result.size() <= limit)
                return result;
            return result.mid(0, limit);
        }

        typedef QPair<QnCameraBookmarkList::const_iterator
            , QnCameraBookmarkList::const_iterator> IteratorsPair;
        typedef std::vector<IteratorsPair> MergeDataVector;

        MergeDataVector mergeData = [nonEmptySources]() -> MergeDataVector
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
        const BinaryPredicate ascPred = [getter](const QnCameraBookmark &first, const QnCameraBookmark &second)
        { return getter(first) < getter(second); };

        const BinaryPredicate descPred = [getter](const QnCameraBookmark &first, const QnCameraBookmark &second)
        { return getter(first) > getter(second); };

        return (isAscending ? ascPred : descPred);
    }

    BinaryPredicate createPredicate(QnCommonModule* commonModule, const QnBookmarkSortOrder &sortOrder)
    {
        const bool isAscending = (sortOrder.order == Qt::AscendingOrder);

        switch(sortOrder.column)
        {
            case Qn::BookmarkName:
            {
                return makePredByGetter(
                    [](const QnCameraBookmark &bookmark)
                    {
                        return bookmark.name;
                    }, isAscending);
            }
            case Qn::BookmarkStartTime:
            {
                return makePredByGetter(
                    [](const QnCameraBookmark &bookmark)
                    {
                        return bookmark.startTimeMs;
                    }, isAscending);
            }
            case Qn::BookmarkDuration:
            {
                return makePredByGetter(
                    [](const QnCameraBookmark &bookmark)
                    {
                        return bookmark.durationMs;
                    }, isAscending);
            }
            case Qn::BookmarkCreationTime:
            {
                return makePredByGetter(
                    [](const QnCameraBookmark &bookmark)
                    {
                        return bookmark.creationTimeMs();
                    }, isAscending);
            }
            case Qn::BookmarkTags:
            {
                static const auto tagsGetter =
                    [](const QnCameraBookmark &bookmark)
                    {
                        return QnCameraBookmark::tagsToString(bookmark.tags);
                    };
                return makePredByGetter(tagsGetter, isAscending);
            }
            case Qn::BookmarkCreator:
            {
                const auto creatorGetter =
                    [commonModule](const QnCameraBookmark& bookmark)
                    {
                        auto resourcePool = commonModule->resourcePool();
                        return helpers::getBookmarkCreatorName(bookmark, resourcePool);
                    };
                return makePredByGetter(creatorGetter, isAscending);
            }
            case Qn::BookmarkCameraName:
            {
                static utils::QnCameraNamesWatcher namesWatcher(commonModule);
                static const auto cameraNameGetter = [](const QnCameraBookmark &bookmark)
                    { return namesWatcher.getCameraName(bookmark.cameraId); };

                return makePredByGetter(cameraNameGetter, isAscending);
            }
            default:
            {
                NX_ASSERT(false, Q_FUNC_INFO, "Invalid bookmark sorting field!");
                return BinaryPredicate();
        }
        };
    };

    QnMultiServerCameraBookmarkList sortEachList(
        QnCommonModule* commonModule,
        QnMultiServerCameraBookmarkList sources,
        const QnBookmarkSortOrder &sortProp)
    {
        for (auto &source: sources)
            QnCameraBookmark::sortBookmarks(commonModule, source, sortProp);

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

    QnCameraBookmarkList getSparseByIters(ItersLinkedList &bookmarkIters
        , int limit)
    {
        NX_ASSERT(limit > 0, Q_FUNC_INFO, "Limit should be greater than 0!");
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
        const QnBookmarkSparsingOptions &sparsing,
        int limit,
        const BinaryPredicate &pred)
    {
        NX_ASSERT(limit > 0, Q_FUNC_INFO, "Limit should be greater than 0!");
        if (limit <= 0)
            return QnCameraBookmarkList();

        if (!sparsing.used || (bookmarks.size() <= limit))
            return bookmarks;

        ItersLinkedList valuableBookmarksIters;
        ItersLinkedList nonValuableBookmarksIters;
        for (auto it = bookmarks.begin(); it != bookmarks.end(); ++it)
        {
            if (it->durationMs >= sparsing.minVisibleLengthMs)
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
}

//

QnBookmarkSortOrder::QnBookmarkSortOrder(Qn::BookmarkSortField column
    , Qt::SortOrder order)
    : column(column)
    , order(order)
{}

const QnBookmarkSortOrder QnBookmarkSortOrder::defaultOrder =
    QnBookmarkSortOrder(Qn::BookmarkStartTime, Qt::AscendingOrder);

//

QnBookmarkSparsingOptions::QnBookmarkSparsingOptions(bool used
    , qint64 minVisibleLengthMs)
    : used(used)
    , minVisibleLengthMs(minVisibleLengthMs)
{}

const QnBookmarkSparsingOptions QnBookmarkSparsingOptions::kNosparsing = QnBookmarkSparsingOptions();

//

qint64 QnCameraBookmark::endTimeMs() const
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

//TODO: #GDM #Bookmarks UNIT TESTS! and future optimization

void QnCameraBookmark::sortBookmarks(
    QnCommonModule* commonModule,
    QnCameraBookmarkList &bookmarks,
    const QnBookmarkSortOrder orderBy)
{
    /* For some reason clang fails to compile this if createPredicate is passed directly to std::sort.
       Using lambda for this works. */
    auto pred = createPredicate(commonModule, orderBy);
    std::sort(bookmarks.begin(), bookmarks.end(), [pred](const QnCameraBookmark &first, const QnCameraBookmark &second)
    {
        return pred(first, second);
    });
}

qint64 QnCameraBookmark::creationTimeMs() const
{
    return isCreatedInOlderVMS()
        ? startTimeMs
        : creationTimeStampMs;
}

bool QnCameraBookmark::isCreatedInOlderVMS() const
{
    return creatorId.isNull();
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
    QnCommonModule* commonModule,
    const QnMultiServerCameraBookmarkList &source,
    const QnBookmarkSortOrder &sortOrder,
    const QnBookmarkSparsingOptions &sparsing,
    int limit)
{
    NX_ASSERT(limit > 0, Q_FUNC_INFO, "Limit should be greater than 0!");
    if (limit <= 0)
        return QnCameraBookmarkList();

    const auto pred = createPredicate(commonModule, sortOrder);

    const int intermediateLimit = (sparsing.used ? QnCameraBookmarkSearchFilter::kNoLimit : limit);
    QnCameraBookmarkList result;
    switch(sortOrder.column)
    {
        case Qn::BookmarkCreationTime:
        case Qn::BookmarkCreator:
        case Qn::BookmarkCameraName:
        case Qn::BookmarkTags:
        {
            const auto bookmarksList = sortEachList(commonModule, source, sortOrder);
            result = mergeSortedBookmarks(bookmarksList, pred, intermediateLimit);
        }
        default:
        {
            // All data by other columns is sorted by database.
            result = mergeSortedBookmarks(source, pred, intermediateLimit);
        }
    }

    if (!sparsing.used)
        return result;

    return getSparseBookmarks(result, sparsing, limit, pred);
}

QnCameraBookmarkTagList QnCameraBookmarkTag::mergeCameraBookmarkTags(const QnMultiServerCameraBookmarkTagList &source, int limit) {
    NX_ASSERT(limit > 0, Q_FUNC_INFO, "Limit must be correct");
    if (limit <= 0)
        return QnCameraBookmarkTagList();

    QnMultiServerCameraBookmarkTagList nonEmptyLists;
    for (const auto &list: source)
        if (!list.isEmpty())
            nonEmptyLists.push_back(list);

    if (nonEmptyLists.empty())
        return QnCameraBookmarkTagList();

    if(nonEmptyLists.size() == 1) {
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
    for (const QnCameraBookmarkTagList &source: nonEmptyLists) {
        for (const auto &tag: source) {
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
        && durationMs > 0;
}

bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other) {
    if (first.startTimeMs == other.startTimeMs)
        return first.guid.toRfc4122() < other.guid.toRfc4122();
    return first.startTimeMs < other.startTimeMs;
}

bool operator<(qint64 first, const QnCameraBookmark &other) {
    return first < other.startTimeMs;
}

bool operator<(const QnCameraBookmark &first, qint64 other) {
    return first.startTimeMs < other;
}

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark) {
    if (bookmark.durationMs > 0)
        dbg.nospace() << "QnCameraBookmark(" << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs).toString(lit("dd hh:mm"))
        << " - " << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs + bookmark.durationMs).toString(lit("dd hh:mm")) << ')';
    else
        dbg.nospace() << "QnCameraBookmark INSTANT (" << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs).toString(lit("dd hh:mm")) << ')';
    dbg.space() << "timeout" << bookmark.timeout;
    dbg.space() << bookmark.name << bookmark.description;
    dbg.space() << QnCameraBookmark::tagsToString(bookmark.tags);
    return dbg.space();
}

QnCameraBookmarkSearchFilter::QnCameraBookmarkSearchFilter():
    startTimeMs(0),
    endTimeMs(std::numeric_limits<qint64>().max()),
    limit(kNoLimit),
    sparsing(),
    orderBy(QnBookmarkSortOrder::defaultOrder)
{}

bool QnCameraBookmarkSearchFilter::isValid() const {
    return startTimeMs <= endTimeMs && limit > 0;
}

bool QnCameraBookmarkSearchFilter::checkBookmark(const QnCameraBookmark &bookmark) const {
    if (bookmark.startTimeMs >= endTimeMs || bookmark.endTimeMs() <= startTimeMs)
        return false;

    if (text.isEmpty())
        return true;

    for (const QString &word: text.split(QRegExp(QLatin1String("[\\W]")), QString::SkipEmptyParts)) {
        bool tagFound = false;

        for (const QString &tag: bookmark.tags) {
            if (tag.startsWith(word, Qt::CaseInsensitive)) {
                tagFound = true;
                break;
            }
        }

        if (tagFound)
            continue;

        QRegExp wordRegExp(QString(QLatin1String("\\b%1")).arg(word), Qt::CaseInsensitive);

        if (wordRegExp.indexIn(bookmark.name) != -1 ||
            wordRegExp.indexIn(bookmark.description) != -1)
        {
            continue;
        }

        return false;
    }

    return true;
}

const int QnCameraBookmarkSearchFilter::kNoLimit = std::numeric_limits<int>::max();

QnCameraBookmarkSearchFilter QnCameraBookmarkSearchFilter::invalidFilter() {
    QnCameraBookmarkSearchFilter filter;
    filter.startTimeMs = 0;
    filter.endTimeMs = -1;
    filter.limit = 0;
    return filter;
}

void serialize_field(const QnCameraBookmarkTags& /*value*/, QVariant* /*target*/) {return ;}
void deserialize_field(const QVariant& /*value*/, QnCameraBookmarkTags* /*target*/) {return ;}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnBookmarkSortOrder)(QnBookmarkSparsingOptions)(QnCameraBookmarkSearchFilter),
    (json)(eq),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnCameraBookmark)(QnCameraBookmarkTag),
    (sql_record)(json)(ubjson)(xml)(csv_record)(eq),
    _Fields)
