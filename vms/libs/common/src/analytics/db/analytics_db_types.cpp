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
#include <nx/utils/log/log_main.h>
#include <api/helpers/camera_id_helper.h>

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
        && equalWithPrecision(rect, right.rect, kCoordinateDecimalDigits)
        && streamIndex == right.streamIndex;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BestShot)(ObjectTrack)(ObjectTrackEx)(ObjectRegion)(ObjectPosition),
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

bool Filter::acceptsAttributes(const Attributes& attributes) const
{
    TextMatcher textMatcher;
    textMatcher.parse(freeText);
    textMatcher.matchAttributes(attributes);
    return textMatcher.matched();
}

bool Filter::acceptsMetadata(const ObjectMetadata& metadata, bool checkBoundingBox) const
{
    return acceptsObjectType(metadata.typeId)
        && (!checkBoundingBox || acceptsBoundingBox(metadata.boundingBox))
        && acceptsAttributes(metadata.attributes);
}

bool Filter::acceptsTrack(
    const ObjectTrack& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    Options options) const
{
    return acceptsTrackInternal(track, objectTypeDictionary, options);
}

bool Filter::acceptsTrackEx(
    const ObjectTrackEx& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    Options options) const
{
    return acceptsTrackInternal(track, objectTypeDictionary, options);
}

template <typename ObjectTrackType>
bool Filter::acceptsTrackInternal(
    const ObjectTrackType& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary,
    Options options) const
{
    using namespace std::chrono;

    if (!objectTrackId.isNull() && track.id != objectTrackId)
        return false;

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

    if (!options.testFlag(Option::ignoreTextFilter))
    {
        TextMatcher textMatcher;
        textMatcher.parse(freeText);
        if (!matchText(&textMatcher, track, objectTypeDictionary))
        {
            if constexpr (std::is_same<decltype(track), const ObjectTrackEx&>::value)
            {
                // Checking the track attributes.
                if (!std::any_of(
                        track.objectPositionSequence.cbegin(),
                        track.objectPositionSequence.cend(),
                        [this, &textMatcher](const ObjectPosition& position)
                        {
                            textMatcher.matchAttributes(position.attributes);
                            return textMatcher.matched();
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
    if (boundingBox && !track.objectPosition.intersect(*boundingBox))
        return false;

    return true;
}

bool Filter::matchText(
    TextMatcher* textMatcher,
    const ObjectTrack& track,
    const AbstractObjectTypeDictionary& objectTypeDictionary) const
{
    const auto objectTypeName = objectTypeDictionary.idToName(track.objectTypeId);
    if (objectTypeName)
        textMatcher->matchText(*objectTypeName);

    textMatcher->matchAttributes(track.attributes);

    return textMatcher->matched();
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

//-------------------------------------------------------------------------------------------------
// Filter::TextMatcher.

void Filter::TextMatcher::parse(const QString& text)
{
    auto filterText = text.trimmed();

    m_exactAttrsToMatch = takeExactAttrMatchFiltersFromText(&filterText);
    m_exactAttrsMatched.clear();
    m_exactAttrsMatched.resize(m_exactAttrsToMatch.size(), false);

    m_attributeToFindNames = takeAttributeToFindNamesFromText(&filterText);
    m_attributeToFindNamesMatched.clear();
    m_attributeToFindNamesMatched.resize(m_attributeToFindNames.size(), false);

    m_tokens = filterText.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    m_tokensMatched.clear();
    m_tokensMatched.resize(m_tokens.size(), false);
}

bool Filter::TextMatcher::empty() const
{
    return m_exactAttrsToMatch.empty()
        && m_attributeToFindNames.empty()
        && m_tokens.empty();
}

void Filter::TextMatcher::matchAttributes(const nx::common::metadata::Attributes& attributes)
{
    if (attributes.empty())
        return;

    matchExactAttributes(attributes);
    checkAttributesPresence(attributes);
    matchAttributeValues(attributes);
}

void Filter::TextMatcher::matchText(const QString& text)
{
    for (std::size_t i = 0; i < m_tokens.size(); ++i)
        m_tokensMatched[i] = m_tokensMatched[i] || text.startsWith(m_tokens[i], Qt::CaseInsensitive);
}

bool Filter::TextMatcher::matched() const
{
    auto notMatched = [](auto val) { return !val; };

    return !std::any_of(m_exactAttrsMatched.begin(), m_exactAttrsMatched.end(), notMatched)
        && !std::any_of(m_attributeToFindNamesMatched.begin(), m_attributeToFindNamesMatched.end(), notMatched)
        && !std::any_of(m_tokensMatched.begin(), m_tokensMatched.end(), notMatched);
}

nx::common::metadata::Attributes Filter::TextMatcher::takeExactAttrMatchFiltersFromText(
    QString* textFilter)
{
    nx::common::metadata::Attributes attributesToMatch;

    QRegularExpression attrRegexp("[\\w\\d\\-_.]+\\s*:\\s*[\\w\\d\\-_.]+");
    for (;;)
    {
        QRegularExpressionMatch match;
        auto pos = textFilter->indexOf(attrRegexp, 0, &match);
        if (pos == -1)
            break;

        auto paramExpr = match.captured();
        textFilter->remove(pos, match.capturedLength());

        const auto tokens = paramExpr.split(":");
        attributesToMatch.push_back(nx::common::metadata::Attribute(
            tokens[0].trimmed(),
            tokens[1].trimmed()));
    }

    return attributesToMatch;
}

void Filter::TextMatcher::matchExactAttributes(
    const nx::common::metadata::Attributes& attributes)
{
    for (std::size_t i = 0; i < m_exactAttrsToMatch.size(); ++i)
    {
        const auto& attrToMatch = m_exactAttrsToMatch[i];

        bool matched = false;
        for (const auto& attr: attributes)
        {
            if (attr.name.startsWith(attrToMatch.name, Qt::CaseInsensitive) &&
                attr.value.startsWith(attrToMatch.value, Qt::CaseInsensitive))
            {
                matched = true;
            }
        }

        m_exactAttrsMatched[i] = m_exactAttrsMatched[i] || matched;
    }
}

std::vector<QString> Filter::TextMatcher::takeAttributeToFindNamesFromText(QString* textFilter)
{
    std::vector<QString> names;

    QRegularExpression attrPresenseRegexp("\\$[\\w\\d\\-_.]+");
    for (;;)
    {
        QRegularExpressionMatch match;
        auto pos = textFilter->indexOf(attrPresenseRegexp, 0, &match);
        if (pos == -1)
            break;

        names.push_back(match.captured().mid(1)); //< Omitting $.
        textFilter->remove(pos, match.capturedLength());
    }

    return names;
}

void Filter::TextMatcher::checkAttributesPresence(
    const nx::common::metadata::Attributes& attributes)
{
    for (std::size_t i = 0; i < m_attributeToFindNames.size(); ++i)
    {
        const auto& name = m_attributeToFindNames[i];

        m_attributeToFindNamesMatched[i] = m_attributeToFindNamesMatched[i] ||
            std::any_of(
                attributes.begin(), attributes.end(),
                [&name](const auto& attr) { return attr.name.startsWith(name, Qt::CaseInsensitive); });
    }
}

void Filter::TextMatcher::matchAttributeValues(
    const nx::common::metadata::Attributes& attributes)
{
    for (std::size_t i = 0; i < m_tokens.size(); ++i)
        m_tokensMatched[i] = m_tokensMatched[i] || wordMatchAnyOfAttributes(m_tokens[i], attributes);
}

bool Filter::TextMatcher::wordMatchAnyOfAttributes(
    const QString& word,
    const nx::common::metadata::Attributes& attributes)
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

//-------------------------------------------------------------------------------------------------

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

    if (filter.needFullTrack)
        params->insert(lit("needFullTrack"), "true");

    params->insert(lit("sortOrder"), QnLexical::serialized(filter.sortOrder));
}

bool deserializeFromParams(const QnRequestParamList& params, Filter* filter,
    QnResourcePool* resourcePool)
{
    const auto kLogTag = nx::utils::log::Tag(std::string("nx::analytics::db::Filter"));

    for (const auto& deviceIdStr: params.allValues(lit("deviceId")))
    {
        // Convert flexibleId to UUID
        auto uuid = nx::camera_id_helper::flexibleIdToId(resourcePool, deviceIdStr);
        if (uuid.isNull())
            uuid = QnUuid::fromStringSafe(deviceIdStr); //< Not have camera in resourcePool.
        if(!uuid.isNull())
            filter->deviceIds.push_back(uuid);
    }

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

    static const QString kNeedFullTrack = "needFullTrack";
    if (params.contains(kNeedFullTrack))
    {
        bool success = false;
        filter->needFullTrack = QnLexical::deserialized<bool>(params.value(kNeedFullTrack), true, &success);
        if (!success)
        {
            NX_WARNING(kLogTag,
                "Invalid value %1 for parameter %2", params.value(kNeedFullTrack), kNeedFullTrack);
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
    os << "needFullTrack " << filter.needFullTrack << "; ";
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
