#include "camera_bookmark.h"

#include <utils/math/defines.h>

#include <utils/common/model_functions.h>

namespace 
{
    QString tagsToString(const QnCameraBookmarkTags &tags, const QString &delimiter) {
        return QStringList(tags.toList()).join(delimiter);
    }
}

qint64 QnCameraBookmark::endTimeMs() const {
    return startTimeMs + durationMs;
}

bool QnCameraBookmark::isNull() const {
    return guid.isNull();
}

QString QnCameraBookmark::tagsAsString(const QString &delimiter) const {
    return tagsToString(tags, delimiter);
}

QString QnCameraBookmark::tagsToString(const QnCameraBookmarkTags &tags, const QString &delimiter) {
    return ::tagsToString(tags, delimiter);
}

//TODO: #GDM #Bookmarks UNIT TESTS! and future optimization
QnCameraBookmarkList QnCameraBookmark::mergeCameraBookmarks(const QnMultiServerCameraBookmarkList &source, int limit, Qn::BookmarkSearchStrategy strategy) {
    Q_ASSERT_X(limit > 0, Q_FUNC_INFO, "Limit must be correct");
    if (limit <= 0)
        return QnCameraBookmarkList();

    QnMultiServerCameraBookmarkList nonEmptyLists;
    for (const auto &list: source)
        if (!list.isEmpty())
            nonEmptyLists.push_back(list);

    if (nonEmptyLists.empty())
        return QnCameraBookmarkList();

    if(nonEmptyLists.size() == 1) {
        QnCameraBookmarkList result = nonEmptyLists.front();
        if (result.size() > limit)
            result.resize(limit);
        return result;
    }

    std::vector< QnCameraBookmarkList::const_iterator > minIndices(nonEmptyLists.size());
    for (size_t i = 0; i < nonEmptyLists.size(); ++i)
        minIndices[i] = nonEmptyLists[i].cbegin();

    QnCameraBookmarkList result;

    int maxSize = 0;
    for (const auto &list: nonEmptyLists)
        maxSize += list.size();
    result.reserve(std::min(maxSize, limit));

    int minIndex = 0;
    while (minIndex != -1) {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        int i = 0;
        for (const QnCameraBookmarkList &periodsList: nonEmptyLists) {
            const auto startIdx = minIndices[i];

            if (startIdx != periodsList.cend()) {
                const QnCameraBookmark &startPeriod = *startIdx;
                if (startPeriod.startTimeMs < minStartTime) {
                    minIndex = i;
                    minStartTime = startPeriod.startTimeMs;
                }
            }
            ++i;
        }

        if (minIndex >= 0) {
            auto &startIdx = minIndices[minIndex];
            const QnCameraBookmark &startPeriod = *startIdx;

            // add chunk to merged data
            if (result.empty()) {
                if (result.size() >= limit && strategy == Qn::EarliestFirst)
                    return result;
                result.push_back(startPeriod);
            } else {
                QnCameraBookmark &last = result.last();
                Q_ASSERT_X(last.startTimeMs <= startPeriod.startTimeMs, Q_FUNC_INFO, "Algorithm semantics failure, order failed");
                if (result.size() >= limit && strategy == Qn::EarliestFirst)
                    return result;
                result.push_back(startPeriod);
            } 
            startIdx++;
        }
    }

    if (strategy == Qn::EarliestFirst || result.size() <= limit)
        return result;

    int offset = result.size() - limit;
    Q_ASSERT_X(offset > 0, Q_FUNC_INFO, "Make sure algorithm is correct");

    switch (strategy) {
    case Qn::LatestFirst: 
        {
            auto insertIter = result.begin();
            auto sourceIter = result.cbegin() + offset;
            while (sourceIter != result.end()) {
                *insertIter = *sourceIter;
                ++insertIter;
                ++sourceIter;
            }
            result.resize(limit);
            break;
        }
    case Qn::LongestFirst: 
        {
            std::partial_sort(result.begin(), result.begin() + offset, result.end(), [](const QnCameraBookmark &l, const QnCameraBookmark &r) {return l.durationMs > r.durationMs; });
            result.resize(limit);
            std::sort(result.begin(), result.end());
            break;
        }
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
    }
    return result;
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
    result.reserve(maxSize);

    //TODO: #dklychkov merge duplicates , sort and cut by limit    
    for (const QnCameraBookmarkTagList &source: nonEmptyLists) {
        for (const auto &tag: source)
            result.push_back(tag);
    }

    if (result.size() > limit)
        result.resize(limit);
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
    dbg.space() << bookmark.tagsAsString();
    return dbg.space();
}

QnCameraBookmarkSearchFilter::QnCameraBookmarkSearchFilter():
    startTimeMs(0),
    endTimeMs(std::numeric_limits<qint64>().max()),
    limit(std::numeric_limits<int>().max()),
    strategy(Qn::EarliestFirst)
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

void serialize_field(const QnCameraBookmarkTags& /*value*/, QVariant* /*target*/) {return ;}
void deserialize_field(const QVariant& /*value*/, QnCameraBookmarkTags* /*target*/) {return ;}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(eq), QnCameraBookmarkSearchFilter_Fields, (optional, true) )

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmark,      (sql_record)(json)(ubjson)(xml)(csv_record)(eq), QnCameraBookmark_Fields,    (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmarkTag,   (sql_record)(json)(ubjson)(xml)(csv_record)(eq), QnCameraBookmarkTag_Fields, (optional, true))
