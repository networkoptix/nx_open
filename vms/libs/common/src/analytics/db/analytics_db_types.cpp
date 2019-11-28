#include "analytics_db_types.h"

#include <QtCore/QRegularExpression>

#include <cmath>
#include <sstream>

#include <common/common_globals.h>
#include <utils/math/math.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

#include "config.h"
#include "analytics_db_utils.h"
#include <nx/streaming/media_data_packet.h>

using namespace nx::common::metadata;

static const int kGridDataSizeBytes = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8;

namespace nx::analytics::db {

namespace {

bool wordMatchAnyOfAttributes(const QString& word, const Attributes& attributes)
{
    std::function<bool(const Attribute&)> wordMatchesAttribute;
    if (word.endsWith('*'))
    {
        const QStringRef prefix = word.leftRef(word.length() - 1);
        wordMatchesAttribute =
            [prefix](const Attribute& attribute)
            {
                return attribute.value.startsWith(prefix, Qt::CaseInsensitive);
            };
    }
    else
    {
        wordMatchesAttribute =
            [&word](const Attribute& attribute)
            {
                return QString::compare(word, attribute.value, Qt::CaseInsensitive) == 0;
            };
    }
    return std::any_of(attributes.cbegin(), attributes.cend(), wordMatchesAttribute);
}

} // namespace


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
        boundingBoxGrid.append(kGridDataSizeBytes, (char)0);
    NX_ASSERT(region.boundingBoxGrid.size(), kGridDataSizeBytes);
    QnMetaDataV1::addMotion(
        (quint64*) this->boundingBoxGrid.data(),
        (quint64*) region.boundingBoxGrid.data());
}

bool ObjectRegion::intersect(const QRectF& rect) const
{
    if (boundingBoxGrid.size() == 0)
        return false;

    simd128i mask[kGridDataSizeBytes / sizeof(simd128i)];
    int maskStart, maskEnd;
    NX_ASSERT(!useSSE2() || ((std::ptrdiff_t)mask) % 16 == 0);
    QnMetaDataV1::createMask(rect, (char*)mask, &maskStart, &maskEnd);
    return QnMetaDataV1::matchImage((quint64*) boundingBoxGrid.data(), mask, maskStart, maskEnd);
}

bool ObjectRegion::isEmpty() const
{
    return boundingBoxGrid.size() == 0;
}

QRectF ObjectRegion::boundingBox() const
{
    QRect rect = QnMetaDataV1::boundingBox(boundingBoxGrid.data());
    return QRectF(
        rect.left() / (float)Qn::kMotionGridWidth,
        rect.top() / (float)Qn::kMotionGridHeight,
        rect.width() / (float)Qn::kMotionGridWidth,
        rect.height() / (float)Qn::kMotionGridHeight);
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
    return memcmp(boundingBoxGrid.data(),
        right.boundingBoxGrid.data(), boundingBoxGrid.size()) == 0;
}

bool BestShot::operator==(const BestShot& right) const
{
    return timestampUs == right.timestampUs
        && equalWithPrecision(rect, right.rect, kCoordinateDecimalDigits);
}

bool ObjectTrack::operator==(const ObjectTrack& right) const
{
    return id == right.id
        && objectTypeId == right.objectTypeId
        //&& attributes == right.attributes
        && firstAppearanceTimeUs == right.firstAppearanceTimeUs
        && lastAppearanceTimeUs == right.lastAppearanceTimeUs
        && objectPosition == right.objectPosition
        && bestShot == right.bestShot;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BestShot)(ObjectTrack)(ObjectRegion)(ObjectPosition),
    (json)(ubjson),
    _analytics_storage_Fields)

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

bool Filter::acceptsObjectType(const QString& typeId) const
{
    return objectTypeId.empty()
        || std::find(objectTypeId.cbegin(), objectTypeId.cend(), typeId) != objectTypeId.cend();
}

bool Filter::acceptsBoundingBox(const QRectF& boundingBox) const
{
    return !this->boundingBox ||
        rectsIntersectToSearchPrecision(*(this->boundingBox), boundingBox);
}

bool Filter::acceptsAttributes(const std::vector<Attribute>& attributes) const
{
    const auto filterWords = freeText.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    if (attributes.empty())
        return filterWords.empty();

    return std::all_of(filterWords.cbegin(), filterWords.cend(),
        [&attributes](const QString& filterWord)
        {
            return wordMatchAnyOfAttributes(filterWord, attributes);
        });
}

bool Filter::acceptsMetadata(const ObjectMetadata& metadata, bool checkBoundingBox) const
{
    return acceptsObjectType(metadata.typeId)
        && (!checkBoundingBox || acceptsBoundingBox(metadata.boundingBox))
        && acceptsAttributes(metadata.attributes);
}

bool Filter::acceptsTrack(const ObjectTrack& track) const
{
     using namespace std::chrono;

    if (!objectTrackId.isNull() && track.id != objectTrackId)
    {
        return false;
    }

    if (!(microseconds(track.lastAppearanceTimeUs) >= timePeriod.startTime() &&
          (timePeriod.isInfinite() ||
              microseconds(track.firstAppearanceTimeUs) < timePeriod.endTime())))
    {
        return false;
    }

    // Matching every device if device list is empty.
    if (!deviceIds.empty() &&
        !nx::utils::contains(deviceIds, track.deviceId))
    {
        return false;
    }

    if (!objectTypeId.empty() &&
        !nx::utils::contains(objectTypeId, track.objectTypeId))
    {
        return false;
    }

    if (!acceptsAttributes(track.attributes))
        return false;

    // Checking the track.
    if (boundingBox && !track.objectPosition.intersect(*boundingBox))
        return false;

    return true;
}

void Filter::loadUserInputToFreeText(const QString& userInput)
{
    freeText = userInputToFreeText(userInput);
}

QString Filter::userInputToFreeText(const QString& userInput)
{
    auto filterWords = userInput.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    for (auto& word: filterWords)
    {
        if (!word.endsWith('*'))
            word = word + '*';
    }
    return filterWords.join(' ');
}

bool Filter::operator==(const Filter& right) const
{
    if (boundingBox.has_value() != right.boundingBox.has_value())
        return false;

    if (boundingBox.has_value() &&
        !equalWithPrecision(*boundingBox, *right.boundingBox, kCoordinateDecimalDigits))
    {
        return false;
    }

    return objectTypeId == right.objectTypeId
        && objectTrackId == right.objectTrackId
        && timePeriod == right.timePeriod
        && freeText == right.freeText
        && sortOrder == right.sortOrder
        && deviceIds == right.deviceIds;
}

bool Filter::operator!=(const Filter& right) const
{
    return !(*this == right);
}

void serializeToParams(const Filter& filter, QnRequestParamList* params)
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
            lit("freeText"),
            QString::fromUtf8(QUrl::toPercentEncoding(filter.freeText)));
    }

    if (filter.maxObjectTracksToSelect > 0)
        params->insert(lit("limit"), QString::number(filter.maxObjectTracksToSelect));

    params->insert(lit("sortOrder"), QnLexical::serialized(filter.sortOrder));
}

bool deserializeFromParams(const QnRequestParamList& params, Filter* filter)
{
    for (const auto& deviceIdStr: params.allValues(lit("deviceId")))
        filter->deviceIds.push_back(QnUuid::fromStringSafe(deviceIdStr));

    for (const auto& objectTypeId: params.allValues(lit("objectTypeId")))
        filter->objectTypeId.push_back(objectTypeId);

    if (params.contains(lit("objectTrackId")))
        filter->objectTrackId = QnUuid::fromStringSafe(params.value(lit("objectTrackId")));

    filter->timePeriod.setStartTime(
        std::chrono::milliseconds(params.value("startTime").toLongLong()));
    filter->timePeriod.setDuration(
        std::chrono::milliseconds(QnTimePeriod::kInfiniteDuration));
    if (params.contains("endTime"))
    {
        filter->timePeriod.setEndTime(
            std::chrono::milliseconds(params.value("endTime").toLongLong()));
    }

    QnLexical::deserialize(params.value(lit("sortOrder")), &filter->sortOrder);

    if (params.contains(lit("x1")))
    {
        if (!(params.contains(lit("y1")) && params.contains(lit("x2")) && params.contains(lit("y2"))))
            return false;

        filter->boundingBox = QRectF(
            QPointF(
                params.value(lit("x1")).toDouble(),
                params.value(lit("y1")).toDouble()),
            QPointF(
                params.value(lit("x2")).toDouble(),
                params.value(lit("y2")).toDouble()));
    }

    if (params.contains(lit("freeText")))
    {
        filter->freeText = QUrl::fromPercentEncoding(
            params.value(lit("freeText")).toUtf8());
    }

    if (params.contains(lit("limit")))
        filter->maxObjectTracksToSelect = params.value(lit("limit")).toInt();

    if (params.contains(lit("maxObjectsToSelect")))
        filter->maxObjectTracksToSelect = params.value(lit("maxObjectsToSelect")).toInt();

    return true;
}

::std::ostream& operator<<(::std::ostream& os, const Filter& filter)
{
    for (const auto& deviceId: filter.deviceIds)
        os << "deviceId " << deviceId.toSimpleString().toStdString() << "; ";
    if (!filter.objectTypeId.empty())
        os << "objectTypeId " << lm("%1").container(filter.objectTypeId).toStdString() << "; ";
    if (!filter.objectTrackId.isNull())
        os << "objectTrackId " << filter.objectTrackId.toSimpleString().toStdString() << "; ";
    os << "timePeriod [" << filter.timePeriod.startTimeMs << ", " <<
        filter.timePeriod.durationMs << "]; ";
    if (filter.boundingBox)
    {
        os << "boundingBox [" <<
            filter.boundingBox->topLeft().x() << ", " <<
            filter.boundingBox->topLeft().y() << ", " <<
            filter.boundingBox->bottomRight().x() << ", " <<
            filter.boundingBox->bottomRight().y() << "]; ";
    }
    if (!filter.freeText.isEmpty())
        os << "freeText \"" << filter.freeText.toStdString() << "\"; ";

    os << "maxObjectsToSelect " << filter.maxObjectTracksToSelect << "; ";
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Filter),
    (json),
    _analytics_storage_Fields,
    (brief, true))

//-------------------------------------------------------------------------------------------------

ResultCode dbResultToResultCode(nx::sql::DBResult dbResult)
{
    switch (dbResult)
    {
        case nx::sql::DBResult::ok:
            return ResultCode::ok;

        case nx::sql::DBResult::retryLater:
            return ResultCode::retryLater;

        default:
            return ResultCode::error;
    }
}

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

using namespace nx::analytics::db;

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::analytics::db, ResultCode,
    (ResultCode::ok, "ok")
    (ResultCode::retryLater, "retryLater")
    (ResultCode::error, "error")
)

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
}
