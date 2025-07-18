// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmark.h"

#include <QtCore/QMap>
#include <QtCore5Compat/QLinkedList>

#include <nx/fusion/model_functions.h>
#include <nx/utils/algorithm/merge_sorted_lists.h>
#include <nx/vms/common/bookmark/bookmark_helpers.h>
#include <nx/vms/common/bookmark/bookmark_sort.h>
#include <nx/vms/common/resource/bookmark_filter.h>
#include <nx/vms/common/system_context.h>
#include <utils/camera/camera_names_watcher.h>
#include <utils/common/synctime.h>

namespace {

using namespace std::chrono;
using namespace nx::vms::common;

using BinaryPredicate =
    std::function<bool (const CameraBookmark& first, const CameraBookmark& second)> ;

MultiServerCameraBookmarkList sortEachList(
    SystemContext* systemContext,
    MultiServerCameraBookmarkList sources,
    const BookmarkSortOrder& sortProp)
{
    const auto resourcePool = systemContext->resourcePool();
    for (auto& source: sources)
        nx::vms::common::sortBookmarks(source, sortProp.column, sortProp.order, resourcePool);

    return sources;
}

using ItersLinkedList = QLinkedList<CameraBookmarkList::const_iterator>;

CameraBookmarkList createListFromIters(const ItersLinkedList& iters)
{
    CameraBookmarkList result;
    result.reserve(iters.size());
    for (auto it: iters)
        result.push_back(*it);
    return result;
}

CameraBookmarkList getSparseByIters(ItersLinkedList& bookmarkIters, int limit)
{
    NX_ASSERT(limit > 0, "Limit should be greater than 0!");
    if (limit <= 0)
        return {};

    if (bookmarkIters.size() <= limit)
        return createListFromIters(bookmarkIters);

    // Thin out bookmarks with calculated step
    const int removeCount = (bookmarkIters.size() - limit);
    const double removeStep =
        static_cast<double>(removeCount) / static_cast<double>(bookmarkIters.size());

    double counter = 0.0;
    double prevCounter = 0.0;
    double removedCounter = 1.0;
    for (auto it = bookmarkIters.begin(); (it != bookmarkIters.end() && bookmarkIters.size() > limit);)
    {
        if (qFuzzyBetween(prevCounter, removedCounter, counter))
        {
            // We have to remove item if next integer counter is reached
            it = bookmarkIters.erase(it);
            removedCounter += 1.0;
        }
        else
            ++it;

        prevCounter = counter;
        counter += removeStep;
    }
    return createListFromIters(bookmarkIters);
}

CameraBookmarkList getSparseBookmarks(
    const CameraBookmarkList& bookmarks,
    const std::optional<milliseconds>& minVisibleLength,
    int limit,
    const BinaryPredicate& pred)
{
    if (!NX_ASSERT(limit > 0, "Limit should be greater than 0!"))
        return {};

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
    MultiServerCameraBookmarkList toMergeLists;
    toMergeLists.push_back(createListFromIters(valuableBookmarksIters));
    toMergeLists.push_back(getSparseByIters(nonValuableBookmarksIters, nonValuableBookmarkToAddCount));

    return nx::utils::algorithm::merge_sorted_lists(std::move(toMergeLists), pred, limit);
}

} // namespace

namespace nx::vms::common {

BookmarkSortOrder::BookmarkSortOrder(
    nx::vms::api::BookmarkSortField column, Qt::SortOrder order):
    column(column), order(order)
{
}

const BookmarkSortOrder BookmarkSortOrder::defaultOrder = BookmarkSortOrder();

const QString CameraBookmark::kGuidParam("guid");
const QString CameraBookmark::kCreationStartTimeParam("creationStartTimeMs");
const QString CameraBookmark::kCreationEndTimeParam("creationEndTimeMs");

bool CameraBookmark::bookmarkMatchesFilter(api::BookmarkShareFilters filters) const
{
    // Only perform share specific filtering if the filter asks for shareable bookmarks.
    if (!filters.testFlag(nx::vms::api::BookmarkShareFilter::shared))
        return true;

    if (filters.testFlag(nx::vms::api::BookmarkShareFilter::hasExpiration))
    {
        if (share.expirationTimeMs == 0ms)
            return false;
    }
    if (filters.testFlag(nx::vms::api::BookmarkShareFilter::expired))
    {
        if (share.expirationTimeMs == 0ms || share.expirationTimeMs >= qnSyncTime->value())
            return false;
    }
    if (filters.testFlag(nx::vms::api::BookmarkShareFilter::accessible))
    {
        if (share.expirationTimeMs != 0ms && share.expirationTimeMs < qnSyncTime->value())
            return false;
    }
    if (filters.testFlag(nx::vms::api::BookmarkShareFilter::passwordProtected))
    {
        if (!share.digest)
            return false;
    }

    return true;
}

milliseconds CameraBookmark::endTime() const
{
    return startTimeMs + durationMs;
}

bool CameraBookmark::isNull() const
{
    return guid.isNull();
}

QString CameraBookmark::tagsToString(const CameraBookmarkTags& tags, const QString& delimiter)
{
    QStringList validTags;
    for (const QString& tag: tags)
    {
        QString trimmed = tag.trimmed();
        if (!trimmed.isEmpty())
            validTags << trimmed;
    }
    return validTags.join(delimiter);
}

milliseconds CameraBookmark::creationTime() const
{
    return creatorId.isNull() ? startTimeMs : creationTimeStampMs;
}

bool CameraBookmark::shareable() const
{
    return share.shareable;
}

bool CameraBookmark::createdBySystem() const
{
    return creatorId == systemUserId();
}

nx::Uuid CameraBookmark::systemUserId()
{
    // Unique UUID used to identify all bookmarks that are create by the system
    return nx::Uuid::fromStringSafe("{51723d00-51bd-4420-8116-75e5f85dfcf4}");
}

CameraBookmarkList CameraBookmark::mergeCameraBookmarks(SystemContext* systemContext,
    const MultiServerCameraBookmarkList& source,
    const BookmarkSortOrder& sortOrder,
    const std::optional<milliseconds>& minVisibleLength,
    int limit)
{
    NX_ASSERT(limit > 0, "Limit should be greater than 0!");
    if (limit <= 0)
        return {};

    const auto pred = createBookmarkSortPredicate(sortOrder.column,
        sortOrder.order,
        systemContext ? systemContext->resourcePool() : nullptr); //< For unit test flexibility.

    const int intermediateLimit =
        minVisibleLength ? CameraBookmarkSearchFilter::kNoLimit : limit;
    CameraBookmarkList result;

    switch (sortOrder.column)
    {
        case nx::vms::api::BookmarkSortField::creationTime:
        case nx::vms::api::BookmarkSortField::creator:
        case nx::vms::api::BookmarkSortField::cameraName:
        case nx::vms::api::BookmarkSortField::tags:
        {
            auto bookmarksList = sortEachList(systemContext, source, sortOrder);
            result =
                nx::utils::algorithm::merge_sorted_lists(std::move(bookmarksList), pred, limit);
            break;
        }
        default:
        {
            // All data by other columns is sorted by database.
            result = nx::utils::algorithm::merge_sorted_lists(source, pred, intermediateLimit);
        }
    }

    if (!minVisibleLength)
        return result;

    return getSparseBookmarks(result, minVisibleLength, limit, pred);
}

CameraBookmarkDb::CameraBookmarkDb(const CameraBookmark& bookmark):
    CameraBookmark(bookmark)
{
}

std::optional<std::chrono::milliseconds> CameraBookmarkDb::shareExpirationTime() const
{
    return share.shareable ? std::make_optional(share.expirationTimeMs) : std::nullopt;
}

void CameraBookmarkDb::setShareExpirationTime(std::optional<std::chrono::milliseconds> value)
{
    share.shareable = value.has_value();
    share.expirationTimeMs = value.value_or(0ms);
}

std::optional<QString> CameraBookmarkDb::shareDigest() const
{
    if (share.shareable && share.digest && !share.digest->isEmpty())
        return share.digest;
    return {};
}

void CameraBookmarkDb::setShareDigest(std::optional<QString> value)
{
    share.digest = std::move(value);
}

CameraBookmarkTagList CameraBookmarkTag::mergeCameraBookmarkTags(
    const MultiServerCameraBookmarkTagList& source, int limit)
{
    NX_ASSERT(limit > 0, "Limit must be correct");
    if (limit <= 0)
        return {};

    MultiServerCameraBookmarkTagList nonEmptyLists;
    for (const auto& list: source)
    {
        if (!list.isEmpty())
            nonEmptyLists.push_back(list);
    }

    if (nonEmptyLists.empty())
        return {};

    if (nonEmptyLists.size() == 1)
    {
        CameraBookmarkTagList result = nonEmptyLists.front();
        if (result.size() > limit)
            result.resize(limit);
        return result;
    }

    CameraBookmarkTagList result;
    int maxSize = 0;
    for (const auto& list: nonEmptyLists)
        maxSize += list.size();
    result.reserve(std::min(limit, maxSize));

    QMap<QString, int> mergedTags;
    for (const auto& list: nonEmptyLists)
    {
        for (const auto& tag: list)
        {
            auto& currentCount = mergedTags[tag.name];
            currentCount += tag.count;
        }
    }

    QMap<int, QString> sortedTags;
    for (auto it = mergedTags.begin(); it != mergedTags.end(); ++it)
        sortedTags.insert(it.value(), it.key());

    for (auto it = sortedTags.begin(); it != sortedTags.end(); ++it)
    {
        if (result.size() >= limit)
            break;

        result.push_front(CameraBookmarkTag{it.value(), it.key()});
    }

    return result;
}

bool CameraBookmark::isValid() const
{
    return !isNull() && !name.isEmpty() && !cameraId.isNull() && durationMs > 0ms;
}

bool operator<(const CameraBookmark& first, const CameraBookmark& other)
{
    if (first.startTimeMs == other.startTimeMs)
        return first.guid.toRfc4122() < other.guid.toRfc4122();
    return first.startTimeMs < other.startTimeMs;
}

bool operator<(milliseconds first, const CameraBookmark& other)
{
    return first < other.startTimeMs;
}

bool operator<(const CameraBookmark& first, milliseconds other)
{
    return first.startTimeMs < other;
}

QDebug operator<<(QDebug dbg, const CameraBookmark& bookmark)
{
    if (bookmark.durationMs > 0ms)
    {
        dbg.nospace() << "CameraBookmark("
                      << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs.count())
                             .toString(lit("dd hh:mm"))
                      << " - "
                      << QDateTime::fromMSecsSinceEpoch(
                             (bookmark.startTimeMs + bookmark.durationMs).count())
                             .toString(lit("dd hh:mm"))
                      << ')';
    }
    else
    {
        dbg.nospace() << "CameraBookmark INSTANT ("
                      << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs.count())
                             .toString(lit("dd hh:mm"))
                      << ')';
    }
    dbg.space() << "timeout" << bookmark.timeout.count();
    dbg.space() << bookmark.name << bookmark.description;
    dbg.space() << CameraBookmark::tagsToString(bookmark.tags);
    return dbg.space();
}

QString CameraBookmarkSearchFilter::toString() const
{
    return QString::fromStdString(nx::reflect::json::serialize(*this));
}

bool CameraBookmarkSearchFilter::isValid() const
{
    if (limit <= 0)
        return false;

    if (startTimeMs != milliseconds::zero() && endTimeMs != milliseconds::zero())
        return startTimeMs <= endTimeMs;

    return true;
}

bool CameraBookmarkSearchFilter::checkBookmark(const CameraBookmark& bookmark) const
{
    if (startTimeMs != milliseconds::zero() && bookmark.endTime() < startTimeMs)
        return false;

    if (endTimeMs != milliseconds::zero() && bookmark.startTimeMs > endTimeMs)
        return false;

    return text.isEmpty() || BookmarkTextFilter(text).match(bookmark);
}

const int CameraBookmarkSearchFilter::kNoLimit = std::numeric_limits<int>::max();

QString bookmarkToString(const CameraBookmark& bookmark)
{
    return bookmark.name;
}

QVariantList bookmarkListToVariantList(const CameraBookmarkList& bookmarks)
{
    QVariantList result;
    for (const auto& b: bookmarks)
        result << QVariant::fromValue(b);
    return result;
}

CameraBookmarkList variantListToBookmarkList(const QVariantList& list)
{
    QnCameraBookmarkList result;
    for  (const auto& v : list)
        result.push_back(v.value<QnCameraBookmark>());
    return result;
}

void serialize_field(const CameraBookmarkTags& /*value*/, QVariant* /*target*/)
{
}
void deserialize_field(const QVariant& /*value*/, CameraBookmarkTags* /*target*/)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkSortOrder, (json), BookmarkSortOrder_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraBookmarkSearchFilter, (json), CameraBookmarkSearchFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkShareableParams,
    (json) (ubjson) (xml) (csv_record),
    BookmarkShareableParams_Fields)

// Explicitly use methods to map SQL columns to inner fields in the structure.
#define VALUE(R, _, ITEM) \
    ((getter, &CameraBookmarkDb::ITEM)(setter, &CameraBookmarkDb::ITEM)\
        (name, BOOST_PP_STRINGIZE(ITEM)))
QN_FUSION_ADAPT_CLASS(CameraBookmarkDb,
    ((getter, &CameraBookmarkDb::shareExpirationTime)(setter, &CameraBookmarkDb::setShareExpirationTime)
        (name, "shareExpirationTimeMs"))
    ((getter, &CameraBookmarkDb::shareDigest)(setter, &CameraBookmarkDb::setShareDigest)
        (name, "shareDigest"))
    BOOST_PP_SEQ_FOR_EACH(VALUE, _, CameraBookmarkDb_Fields)
    )
#undef VALUE

QN_FUSION_DEFINE_FUNCTIONS(CameraBookmarkDb, (sql_record))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraBookmark, (json) (ubjson) (xml) (csv_record), CameraBookmark_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraBookmarkWithRecordId, (json) (ubjson) (xml) (csv_record),
    CameraBookmarkWithRowId_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CameraBookmarkTag,
    (sql_record) (json) (ubjson) (xml) (csv_record), CameraBookmarkTag_Fields)

} // namespace nx::vms::common
