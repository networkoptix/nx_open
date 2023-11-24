// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_db_types.h"

#include <QtCore/QRegularExpression>

#include <cmath>
#include <sstream>

#include <api/helpers/camera_id_helper.h>
#include <common/common_globals.h>
#include <common/common_module.h>
#include <utils/math/math.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/rest/params.h>
#include <nx/reflect/string_conversion.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/std/algorithm.h>
#include <nx/network/rest/params.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/analytics/taxonomy/helpers.h>

#include "analytics_db_utils.h"
#include "config.h"

using namespace nx::common::metadata;

static const int kGridDataSizeBytes = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8;

namespace nx::analytics::db {

void ObjectRegion::add(const QRectF& rect)
{
    if (boundingBoxGrid.size() == 0)
        boundingBoxGrid.append(kGridDataSizeBytes, (char) 0);

    QnMetaDataV1::addMotion(this->boundingBoxGrid.data(), rect);
}

void ObjectRegion::add(const ObjectRegion& region)
{
    if (region.boundingBoxGrid.isEmpty())
        return;
    if (boundingBoxGrid.size() == 0)
        boundingBoxGrid.append(kGridDataSizeBytes, (char) 0);
    NX_ASSERT(region.boundingBoxGrid.size(), kGridDataSizeBytes);
    QnMetaDataV1::addMotion(
        (quint64*) this->boundingBoxGrid.data(), (quint64*) region.boundingBoxGrid.data());
}

bool ObjectRegion::intersect(const QRectF& rect) const
{
    if (boundingBoxGrid.size() == 0)
        return false;

    simd128i mask[kGridDataSizeBytes / sizeof(simd128i)];
    int maskStart, maskEnd;
    NX_ASSERT(!useSSE2() || ((std::ptrdiff_t) mask) % 16 == 0);
    QnMetaDataV1::createMask(rect, (char*) mask, &maskStart, &maskEnd);
    return QnMetaDataV1::matchImage((quint64*) boundingBoxGrid.data(), mask, maskStart, maskEnd);
}

bool ObjectRegion::isEmpty() const
{
    return boundingBoxGrid.size() == 0;
}

QRect ObjectRegion::boundingBox() const
{
    return QnMetaDataV1::boundingBox(boundingBoxGrid.data());
}

bool ObjectRegion::isSimpleRect() const
{
    if (boundingBoxGrid.size() == 0)
        return false;
    QRect rect = QnMetaDataV1::boundingBox(boundingBoxGrid.data());
    for (int y = rect.top(); y <= rect.bottom(); ++y)
    {
        for (int x = rect.left(); x <= rect.right(); ++x)
        {
            if (!QnMetaDataV1::isMotionAt(x, y, boundingBoxGrid.data()))
                return false;
        }
    }
    return true;
}

void ObjectRegion::clear()
{
    boundingBoxGrid.clear();
}

bool ObjectRegion::operator==(const ObjectRegion& right) const
{
    if (boundingBoxGrid.size() != right.boundingBoxGrid.size())
        return false;
    return memcmp(boundingBoxGrid.data(), right.boundingBoxGrid.data(), boundingBoxGrid.size())
        == 0;
}

ObjectTrackEx::ObjectTrackEx(const ObjectTrack& data)
{
    id = data.id;
    deviceId = data.deviceId;
    objectTypeId = data.objectTypeId;
    attributes = data.attributes;
    firstAppearanceTimeUs = data.firstAppearanceTimeUs;
    lastAppearanceTimeUs = data.lastAppearanceTimeUs;
    objectPosition = data.objectPosition;
    bestShot = data.bestShot;
    analyticsEngineId = data.analyticsEngineId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Image, (json)(ubjson), Image_analytics_storage_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BestShot, (json)(ubjson), BestShot_analytics_storage_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BestShotEx, (json)(ubjson), BestShotEx_analytics_storage_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectTrack, (json)(ubjson), ObjectTrack_analytics_storage_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectTrackEx, (json)(ubjson), ObjectTrackEx_analytics_storage_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectRegion, (json)(ubjson), ObjectRegion_analytics_storage_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectPosition, (json)(ubjson), ObjectPosition_analytics_storage_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Region, (json), Region_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MotionFilter, (json), MotionFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TimePeriodsLookupOptions, (json), (detailLevel))

//-------------------------------------------------------------------------------------------------

Filter::Filter()
{
    // NOTE: The default time period constructor provides an empty time period.
    timePeriod.setEndTimeMs(QnTimePeriod::kMaxTimeValue);
}

bool Filter::empty() const
{
    return *this == Filter();
}

bool Filter::acceptsMetadata(const QnUuid& deviceId,
    const nx::common::metadata::ObjectMetadata& metadata,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    bool checkBoundingBox) const
{
    if (checkBoundingBox && boundingBox
        && !rectsIntersectToSearchPrecision(*boundingBox, metadata.boundingBox))
    {
        return false;
    }

    ObjectTrack track;
    track.id = metadata.trackId;
    track.deviceId = deviceId;
    track.objectTypeId = metadata.typeId;
    track.analyticsEngineId = metadata.analyticsEngineId;
    track.attributes = metadata.attributes;
    return acceptsTrackInternal(track,
        objectTypeDictionary,
        Options(Option::ignoreBoundingBox | Option::ignoreTimePeriod));
}

bool Filter::acceptsTrack(const ObjectTrack& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    Options options) const
{
    return acceptsTrackInternal(track, objectTypeDictionary, options);
}

bool Filter::acceptsTrackEx(const ObjectTrackEx& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    Options options) const
{
    return acceptsTrackInternal(track, objectTypeDictionary, options);
}

template<typename ObjectTrackType>
bool Filter::acceptsTrackInternal(const ObjectTrackType& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    Options options) const
{
    using namespace std::chrono;

    if (!objectTrackId.isNull() && track.id != objectTrackId)
        return false;

    if (!options.testFlag(Option::ignoreTimePeriod))
    {
        if (!(microseconds(track.lastAppearanceTimeUs) >= timePeriod.startTime()
                && (timePeriod.isInfinite()
                    || microseconds(track.firstAppearanceTimeUs) < timePeriod.endTime())))
        {
            return false;
        }
    }

    // Matching every device if device list is empty.
    if (!deviceIds.empty() && !nx::utils::contains(deviceIds, track.deviceId))
    {
        return false;
    }

    if (!objectTypeId.empty() && !nx::utils::contains(objectTypeId, track.objectTypeId))
    {
        return false;
    }

    if (!analyticsEngineId.isNull() && track.analyticsEngineId != analyticsEngineId)
        return false;

    if (!freeText.isEmpty() && !options.testFlag(Option::ignoreTextFilter))
    {
        TextMatcher tempTextMatcher;
        const TextMatcher* textMatcher = context ? &context->textMatcher : &tempTextMatcher;
        if (!context)
            tempTextMatcher.parse(freeText);

        if (!matchText(textMatcher, track, objectTypeDictionary))
        {
            if constexpr (std::is_same<decltype(track), const ObjectTrackEx&>::value)
            {
                // Checking the track attributes.
                if (!std::any_of(track.objectPositionSequence.cbegin(),
                        track.objectPositionSequence.cend(),
                        [this, &textMatcher](const ObjectPosition& position) {
                            return textMatcher->matchAttributes(position.attributes);
                        }))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }

    // Checking the track.
    if (!options.testFlag(Option::ignoreBoundingBox))
    {
        if (boundingBox && !track.objectPosition.intersect(*boundingBox))
            return false;
    }

    return true;
}

bool Filter::matchText(const TextMatcher* textMatcher,
    const ObjectTrack& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary) const
{
    const auto objectTypeName = objectTypeDictionary.idToName(track.objectTypeId);
    if (objectTypeName && textMatcher->matchText(*objectTypeName))
        return true;

    return textMatcher->matchAttributes(track.attributes);
}

bool Filter::operator==(const Filter& right) const
{
    if (boundingBox.has_value() != right.boundingBox.has_value())
        return false;

    if (boundingBox.has_value()
        && !equalWithPrecision(*boundingBox, *right.boundingBox, kCoordinateDecimalDigits))
    {
        return false;
    }

    return objectTypeId == right.objectTypeId
        && objectTrackId == right.objectTrackId
        && analyticsEngineId == right.analyticsEngineId
        && timePeriod == right.timePeriod
        && freeText == right.freeText
        && sortOrder == right.sortOrder
        && deviceIds == right.deviceIds;
}

bool Filter::operator!=(const Filter& right) const
{
    return !(*this == right);
}

//-------------------------------------------------------------------------------------------------

void serializeToParams(const Filter& filter, nx::network::rest::Params* params)
{
    for (const auto deviceId: filter.deviceIds)
        params->insert(lit("deviceId"), deviceId.toSimpleString());

    for (const auto& objectTypeId: filter.objectTypeId)
        params->insert(lit("objectTypeId"), objectTypeId);

    if (!filter.objectTrackId.isNull())
        params->insert(lit("objectTrackId"), filter.objectTrackId.toSimpleString());

    params->insert(lit("startTime"), QnLexical::serialized(filter.timePeriod.startTimeMs));
    params->insert(lit("endTime"), QnLexical::serialized(filter.timePeriod.endTimeMs()));

    if (filter.boundingBox)
    {
        params->insert(lit("x1"), QString::number(filter.boundingBox->topLeft().x()));
        params->insert(lit("y1"), QString::number(filter.boundingBox->topLeft().y()));
        params->insert(lit("x2"), QString::number(filter.boundingBox->bottomRight().x()));
        params->insert(lit("y2"), QString::number(filter.boundingBox->bottomRight().y()));
    }

    if (!filter.freeText.isEmpty())
    {
        params->insert(
            lit("freeText"), QString::fromUtf8(QUrl::toPercentEncoding(filter.freeText)));
    }

    if (!filter.analyticsEngineId.isNull())
        params->insert("analyticsEngineId", filter.analyticsEngineId.toSimpleString());

    if (filter.maxObjectTracksToSelect > 0)
        params->insert(lit("limit"), QString::number(filter.maxObjectTracksToSelect));

    if (filter.needFullTrack)
        params->insert(lit("needFullTrack"), "true");

    params->insert("sortOrder", nx::reflect::toString(filter.sortOrder));
}

bool loadFromUrlQuery(
    const QUrlQuery& urlQuery,
    Filter* filter,
    const QnResourcePool* resourcePool,
    const nx::analytics::taxonomy::AbstractStateWatcher* taxonomyStateWatcher)
{
    return deserializeFromParams(
        nx::network::rest::Params::fromUrlQuery(urlQuery), filter, resourcePool, taxonomyStateWatcher);
}

void serializeToUrlQuery(const Filter& filter, nx::utils::UrlQuery& urlQuery)
{
    auto paramList = std::make_unique<nx::network::rest::Params>();
    serializeToParams(filter, paramList.get());
    urlQuery = nx::utils::UrlQuery(paramList->toUrlQuery());
}

std::set<QString> addDerivedTypeIds(
    const nx::analytics::taxonomy::AbstractStateWatcher* stateWatcher,
    const QList<QString>& objectTypeIdsFromUser)
{
    using namespace nx::analytics::taxonomy;

    std::set<QString> parentObjectTypeIds;
    for (const auto& objectTypeId: objectTypeIdsFromUser)
        parentObjectTypeIds.insert(objectTypeId);

    if (!NX_ASSERT(stateWatcher, "Unable to access the analytics taxonomy state watcher"))
        return parentObjectTypeIds;

    const std::shared_ptr<AbstractState> state = stateWatcher->state();
    if (!NX_ASSERT(state, "Unable to access the analytics taxonomy state"))
        return parentObjectTypeIds;

    std::set<QString> result = parentObjectTypeIds;
    for (const QString& objectTypeId: parentObjectTypeIds)
    {
        const std::set<QString> derivedObjectTypeIds =
            getAllDerivedTypeIds(state.get(), objectTypeId);

        result.insert(derivedObjectTypeIds.cbegin(), derivedObjectTypeIds.cend());
    }

    return result;
}

bool deserializeFromParams(
    const nx::network::rest::Params& params,
    Filter* filter,
    const QnResourcePool* resourcePool,
    const nx::analytics::taxonomy::AbstractStateWatcher* taxonomyStateWatcher)
{
    const auto kLogTag = nx::utils::log::Tag(std::string("nx::analytics::db::Filter"));

    for (const auto& deviceIdStr: params.values(lit("deviceId")))
    {
        // Convert flexibleId to UUID
        QnUuid uuid;
        if (resourcePool)
            uuid = nx::camera_id_helper::flexibleIdToId(resourcePool, deviceIdStr);
        if (uuid.isNull())
            uuid = QnUuid::fromStringSafe(deviceIdStr); //< Not have camera in resourcePool.
        if (!uuid.isNull())
            filter->deviceIds.insert(uuid);
    }

    if (taxonomyStateWatcher)
    {
        filter->objectTypeId = addDerivedTypeIds(taxonomyStateWatcher, params.values("objectTypeId"));
    }
    else
    {
        for (const auto& objectTypeId: params.values("objectTypeId"))
            filter->objectTypeId.insert(objectTypeId);
    }

    if (params.contains(lit("objectTrackId")))
        filter->objectTrackId = QnUuid::fromStringSafe(params.value(lit("objectTrackId")));

    filter->timePeriod.setStartTime(
        std::chrono::milliseconds(params.value("startTime").toLongLong()));
    filter->timePeriod.setDuration(std::chrono::milliseconds(QnTimePeriod::kInfiniteDuration));
    if (params.contains("endTime"))
    {
        filter->timePeriod.setEndTime(
            std::chrono::milliseconds(params.value("endTime").toLongLong()));
    }

    nx::reflect::fromString(params.value("sortOrder").toStdString(), &filter->sortOrder);

    if (params.contains(lit("x1")))
    {
        if (!(params.contains(lit("y1")) && params.contains(lit("x2"))
                && params.contains(lit("y2"))))
            return false;

        filter->boundingBox =
            QRectF(QPointF(params.value(lit("x1")).toDouble(), params.value(lit("y1")).toDouble()),
                QPointF(params.value(lit("x2")).toDouble(), params.value(lit("y2")).toDouble()));
    }

    if (params.contains(lit("freeText")))
    {
        filter->freeText = QUrl::fromPercentEncoding(params.value(lit("freeText")).toUtf8());
    }

    if (params.contains("analyticsEngineId"))
        filter->analyticsEngineId = QnUuid::fromStringSafe(params.value("analyticsEngineId"));

    if (params.contains(lit("limit")))
        filter->maxObjectTracksToSelect = params.value(lit("limit")).toInt();

    if (params.contains(lit("maxObjectsToSelect")))
        filter->maxObjectTracksToSelect = params.value(lit("maxObjectsToSelect")).toInt();

    static const QString kNeedFullTrack = "needFullTrack";
    if (params.contains(kNeedFullTrack))
    {
        bool success = false;
        filter->needFullTrack =
            QnLexical::deserialized<bool>(params.value(kNeedFullTrack), true, &success);
        if (!success)
        {
            NX_WARNING(kLogTag,
                "Invalid value %1 for parameter %2",
                params.value(kNeedFullTrack),
                kNeedFullTrack);
            return false;
        }
    }

    return true;
}

::std::ostream& operator<<(::std::ostream& os, const Filter& filter)
{
    for (const auto& deviceId: filter.deviceIds)
        os << "deviceId " << deviceId.toSimpleString().toStdString() << "; ";
    if (!filter.objectTypeId.empty())
        os << "objectTypeId " << nx::format("%1").container(filter.objectTypeId).toStdString() << "; ";
    if (!filter.objectTrackId.isNull())
        os << "objectTrackId " << filter.objectTrackId.toSimpleString().toStdString() << "; ";
    os << "timePeriod [" << filter.timePeriod.startTimeMs << ", " << filter.timePeriod.durationMs
       << "]; ";
    if (filter.boundingBox)
    {
        os << "boundingBox [" << filter.boundingBox->topLeft().x() << ", "
           << filter.boundingBox->topLeft().y() << ", " << filter.boundingBox->bottomRight().x()
           << ", " << filter.boundingBox->bottomRight().y() << "]; ";
    }
    if (!filter.freeText.isEmpty())
        os << "freeText \"" << filter.freeText.toStdString() << "\"; ";

    os << "maxObjectsToSelect " << filter.maxObjectTracksToSelect << "; ";
    os << "needFullTrack " << filter.needFullTrack << "; ";

    if (!filter.analyticsEngineId.isNull())
        os << "analyticsEngineId " << filter.analyticsEngineId.toSimpleString().toStdString() << "; ";

    os << "sortOrder " <<
        (filter.sortOrder == Qt::SortOrder::DescendingOrder ? "DESC" : "ASC");

    return os;
}

QString toString(const Filter& filter)
{
    std::ostringstream stringStream;
    stringStream << filter;
    return QString::fromStdString(stringStream.str());
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Filter, (json), Filter_analytics_storage_Fields, (brief, true))

//-------------------------------------------------------------------------------------------------

nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::ok:
            return nx::network::http::StatusCode::ok;
        case ResultCode::retryLater:
            return nx::network::http::StatusCode::serviceUnavailable;
        case ResultCode::error:
            return nx::network::http::StatusCode::internalServerError;
        default:
            return nx::network::http::StatusCode::internalServerError;
    }
}

ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode)
{
    if (nx::network::http::StatusCode::isSuccessCode(statusCode))
        return ResultCode::ok;

    switch (statusCode)
    {
        case nx::network::http::StatusCode::serviceUnavailable:
            return ResultCode::retryLater;
        case nx::network::http::StatusCode::internalServerError:
            return ResultCode::error;
        default:
            return ResultCode::error;
    }
}

} // namespace nx::analytics::db
