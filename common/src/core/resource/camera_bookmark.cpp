#include "camera_bookmark.h"

#include <QtCore/QMap>

#include <utils/common/model_functions.h>

namespace
{
    //TODO: #ynikitenkov make generic version of algorithm

    typedef std::function<bool (const QnCameraBookmark &first, const QnCameraBookmark &second)> BinaryPredicate;
    QnCameraBookmarkList mergeSortedBookmarks(const QnMultiServerCameraBookmarkList &source
        , const BinaryPredicate &pred
        , int limit)
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
            return *nonEmptySources.front();

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
            const QnCameraBookmark *minBookmark = &*mergeDataIt->first;
            for (auto it = mergeDataIt + 1; it != mergeData.end(); ++it)
            {
                const auto &currentBookmark = *it->first;
                if (!pred(*minBookmark, currentBookmark))
                    continue;

                mergeDataIt = it;
                minBookmark = &currentBookmark;
            }

            result.append(*minBookmark);
            if (++mergeDataIt->first == mergeDataIt->second)    // if list with min element is logically empty
                mergeData.erase(mergeDataIt);

            // TODO: add copy elements if it is only one list
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

    BinaryPredicate createPredicate(const QnBookmarkSortProps &sortProperties)
    {
        const bool isAscending = (sortProperties.order == Qn::Ascending);

        switch(sortProperties.column)
        {
        case Qn::BookmarkName:
            return makePredByGetter([](const QnCameraBookmark &bookmark) { return bookmark.name; }, isAscending);
        case Qn::BookmarkStartTime:
            return makePredByGetter([](const QnCameraBookmark &bookmark) { return bookmark.startTimeMs; } , isAscending);
        case Qn::BookmarkDuration:
            return makePredByGetter([](const QnCameraBookmark &bookmark) { return bookmark.endTimeMs(); } , isAscending);
            // case Qn::BookmarkTags:           // TODO: $ynikitenkov Implement me
            // case Qn::BookmarkCameraName:     // TODO:
        default:
            Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid bookmark sorting column!");
            return BinaryPredicate();
        };
    };

    QnMultiServerCameraBookmarkList sortEachList(QnMultiServerCameraBookmarkList sources
        , const QnBookmarkSortProps &sortProp)
    {
        for (auto &source: sources)
            QnCameraBookmark::sortBookmarks(source, sortProp);

        return std::move(sources);
    }
}

qint64 QnCameraBookmark::endTimeMs() const
{
    return startTimeMs + durationMs;
}

bool QnCameraBookmark::isNull() const
{
    return guid.isNull();
}

QnBookmarkSortProps::QnBookmarkSortProps(Qn::BookmarkSortColumn column
    , Qn::SortOrder order)
    : column(column)
    , order(order)
{}

const QnBookmarkSortProps QnBookmarkSortProps::default =
    QnBookmarkSortProps(Qn::BookmarkStartTime, Qn::Ascending);

QString QnCameraBookmark::tagsToString(const QnCameraBookmarkTags &bokmarkTags, const QString &delimiter)
{
    return QStringList(bokmarkTags.toList()).join(delimiter);
}

//TODO: #GDM #Bookmarks UNIT TESTS! and future optimization

void QnCameraBookmark::sortBookmarks(QnCameraBookmarkList &bookmarks
    , const QnBookmarkSortProps sortProps)
{
    std::sort(bookmarks.begin(), bookmarks.end(), createPredicate(sortProps));
}

QnCameraBookmarkList QnCameraBookmark::mergeCameraBookmarks(const QnMultiServerCameraBookmarkList &source
    , const QnBookmarkSortProps &sortProperties
    , int limit)
{
    const auto pred = createPredicate(sortProperties);

    if (sortProperties.column ==Qn::BookmarkCameraName)
        mergeSortedBookmarks(sortEachList(source, sortProperties), pred, limit);
    else if (sortProperties.column == Qn::BookmarkTags)
        mergeSortedBookmarks(sortEachList(source, sortProperties), pred, limit);

    // We have several properly sorted bookmark lists here.
    // So, we just have to merge them
    return mergeSortedBookmarks(source, pred, limit);
}

QnCameraBookmarkTagList QnCameraBookmarkTag::mergeCameraBookmarkTags(const QnMultiServerCameraBookmarkTagList &source, int limit) {
    Q_ASSERT_X(limit > 0, Q_FUNC_INFO, "Limit must be correct");
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
    return !isNull() && !cameraId.isEmpty();
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
    limit(std::numeric_limits<int>().max()),
    sortProps(QnBookmarkSortProps::default)
{}

bool QnCameraBookmarkSearchFilter::isValid() const {
    return startTimeMs <= endTimeMs && limit > 0;
}

bool QnCameraBookmarkSearchFilter::checkBookmark(const QnCameraBookmark &bookmark) const {
    if (bookmark.startTimeMs >= endTimeMs || bookmark.endTimeMs() <= startTimeMs)
        return false;

    if (text.isEmpty())
        return true;

    for (const QString &word: text.split(QRegExp(lit("[\\W]")), QString::SkipEmptyParts)) {
        bool tagFound = false;

        for (const QString &tag: bookmark.tags) {
            if (tag.startsWith(word, Qt::CaseInsensitive)) {
                tagFound = true;
                break;
            }
        }

        if (tagFound)
            continue;

        QRegExp wordRegExp(lit("\\b%1").arg(word), Qt::CaseInsensitive);

        if (wordRegExp.indexIn(bookmark.name) != -1 ||
            wordRegExp.indexIn(bookmark.description) != -1)
        {
            continue;
        }

        return false;
    }

    return true;
}

QnCameraBookmarkSearchFilter QnCameraBookmarkSearchFilter::invalidFilter() {
    QnCameraBookmarkSearchFilter filter;
    filter.startTimeMs = 0;
    filter.endTimeMs = -1;
    filter.limit = 0;
    return filter;
}

void serialize_field(const QnCameraBookmarkTags& /*value*/, QVariant* /*target*/) {return ;}
void deserialize_field(const QVariant& /*value*/, QnCameraBookmarkTags* /*target*/) {return ;}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBookmarkSortProps, (json)(eq), QnBookmarkSortProps_Fields, (optional, true) )
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(eq), QnCameraBookmarkSearchFilter_Fields, (optional, true) )

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmark,      (sql_record)(json)(ubjson)(xml)(csv_record)(eq), QnCameraBookmark_Fields,    (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmarkTag,   (sql_record)(json)(ubjson)(xml)(csv_record)(eq), QnCameraBookmarkTag_Fields, (optional, true))
