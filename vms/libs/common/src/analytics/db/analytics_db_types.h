#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <QtGui/QRegion>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/sql/types.h>

#include <analytics/common/object_metadata.h>
#include <recording/time_period.h>
#include <utils/common/request_param.h>
#include <utils/common/byte_array.h>

#include "abstract_object_type_dictionary.h"

class QnResourcePool;

namespace nx::analytics::db {

struct ObjectPosition
{
    /** Device the object has been detected on. */
    QnUuid deviceId;

    qint64 timestampUs = 0;
    qint64 durationUs = 0;
    QRectF boundingBox;

    /** Variable object attributes. E.g., car speed. */
    nx::common::metadata::Attributes attributes;
};

#define ObjectPosition_analytics_storage_Fields \
    (deviceId)(timestampUs)(durationUs)(boundingBox)(attributes)
QN_FUSION_DECLARE_FUNCTIONS(ObjectPosition, (json)(ubjson));

struct ObjectRegion
{
    QByteArray boundingBoxGrid;

    void add(const QRectF& rect);
    void add(const ObjectRegion& region);
    bool intersect(const QRectF& rect) const;
    bool isEmpty() const;
    QRectF boundingBox() const;
    bool isSimpleRect() const;
    void clear();

    bool operator==(const ObjectRegion& right) const;
};

#define ObjectRegion_analytics_storage_Fields (boundingBoxGrid)
QN_FUSION_DECLARE_FUNCTIONS(ObjectRegion, (json)(ubjson));

struct BestShot
{
    qint64 timestampUs = 0;
    QRectF rect;
    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;

    bool initialized() const { return timestampUs > 0; }

    bool operator==(const BestShot& right) const;
};

#define BestShot_analytics_storage_Fields \
    (timestampUs)(rect)(streamIndex)

QN_FUSION_DECLARE_FUNCTIONS(BestShot, (json)(ubjson));

struct ObjectTrack
{
    /** Device object has been detected on. */
    QnUuid id;
    QnUuid deviceId;
    QString objectTypeId; //< TODO: #rvasilenko Rename to bestShotObjectTypeId
    nx::common::metadata::Attributes attributes; //< TODO: #rvasilenko Rename to bestShotAttributes
    qint64 firstAppearanceTimeUs = 0;
    qint64 lastAppearanceTimeUs = 0;
    ObjectRegion objectPosition;
    BestShot bestShot;
};

struct ObjectTrackEx: public ObjectTrack
{
    std::vector<ObjectPosition> objectPositionSequence;

    ObjectTrackEx() = default;
    ObjectTrackEx(const ObjectTrack& data);
};

#define ObjectTrack_analytics_storage_Fields \
    (id)(deviceId)(objectTypeId)(attributes)(firstAppearanceTimeUs) \
    (lastAppearanceTimeUs)(objectPosition)(bestShot)

#define ObjectTrackEx_analytics_storage_Fields \
    ObjectTrack_analytics_storage_Fields (objectPositionSequence)

QN_FUSION_DECLARE_FUNCTIONS(ObjectTrack, (json)(ubjson));
QN_FUSION_DECLARE_FUNCTIONS(ObjectTrackEx, (json)(ubjson));

//-------------------------------------------------------------------------------------------------

struct Filter
{
    enum Option
    {
        none = 0x0,
        ignoreTextFilter = 0x1,
    };

    Q_DECLARE_FLAGS(Options, Option)

    /** If empty, any device is matched. */
    std::vector<QnUuid> deviceIds;

    // TODO: #mshevchenko Why 'Id' and not `Ids`? And why not `nx::analytics::ObjectTypeId`?
    std::vector<QString> objectTypeId;

    QnUuid objectTrackId;
    QnTimePeriod timePeriod;

    /** Coordinates in range [0;1]. */
    std::optional<QRectF> boundingBox;

    /**
     * Set of words separated by spaces, commas, etc. The search is done across all attribute
     * values, using wildcards.
     */
    QString freeText;

    // TODO: #ak Move result options to a separate struct.

    /** Zero value is treated as no limit. */
    int maxObjectTracksToSelect = 0;

    /** If true, track details (geometry data) will be selected. */
    bool needFullTrack = false;

    /** Found tracks are sorted by the minimum track time using this order. */
    Qt::SortOrder sortOrder = Qt::SortOrder::DescendingOrder;

    Filter();

    bool empty() const;

    bool acceptsObjectType(const QString& typeId) const;
    bool acceptsBoundingBox(const QRectF& boundingBox) const;
    bool acceptsAttributes(const nx::common::metadata::Attributes& attributes) const;
    bool acceptsMetadata(const nx::common::metadata::ObjectMetadata& metadata,
        bool checkBoundingBox = true) const;

    bool acceptsTrack(
        const ObjectTrack& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        Options options = Option::none) const;

    bool acceptsTrackEx(
        const ObjectTrackEx& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        Options options = Option::none) const;

    /**
     * Search is implemented by attribute values only. SqLite fts4 syntax supports only full match
     * or prefix match, so we append `*` to each word of the user input to enable prefix lookup.
     */
    void loadUserInputToFreeText(const QString& userInput);

    /**
     * Search is implemented by attribute values only. SqLite fts4 syntax supports only full match
     * or prefix match, so we append `*` to each word of the user input to enable prefix lookup.
     */
    static QString userInputToFreeText(const QString& userInput);

    bool operator==(const Filter& right) const;
    bool operator!=(const Filter& right) const;

private:
    /**
     * To use this class first parse the text filter.
     * Format description:
     * TextFilter = *matchToken
     * matchToken = word | attributeName | attributeToken
     * attributeName = "$" word
     * attributeToken = word ":" word
     * word = 1 * (DIGIT | ALPHA) | '"' TEXT '"'
     *
     * Then, invoke matchAttributes() or mathText() and check the match result with matched().
     * matched() reports true only if all tokens were matched
     */
    class TextMatcher
    {
    public:
        void parse(const QString& text);
        bool empty() const;

        void matchAttributes(const nx::common::metadata::Attributes& attributes);
        void matchText(const QString& text);

        /**
         * @return true If all tokens of the filter were matched
         * by calls to matchAttributes or matchText().
         */
        bool matched() const;

    private:
        static nx::common::metadata::Attributes takeExactAttrMatchFiltersFromText(
            QString* textFilter);

        void matchExactAttributes(
            const nx::common::metadata::Attributes& attributes);

        static std::vector<QString> takeAttributeToFindNamesFromText(QString* textFilter);

        void checkAttributesPresence(
            const nx::common::metadata::Attributes& attributes);

        void matchAttributeValues(
            const nx::common::metadata::Attributes& attributes);

        bool wordMatchAnyOfAttributes(
            const QString& word,
            const nx::common::metadata::Attributes& attributes);

    private:
        nx::common::metadata::Attributes m_exactAttrsToMatch;
        /** Each element is set to true when corresponding element of m_exactAttrsToMatch was matched. */
        std::vector<bool> m_exactAttrsMatched;

        std::vector<QString> m_attributeToFindNames;
        std::vector<bool> m_attributeToFindNamesMatched;

        QStringList m_tokens;
        std::vector<bool> m_tokensMatched;
    };

    template <typename ObjectTrackType>
    bool acceptsTrackInternal(
        const ObjectTrackType& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary,
        Options options) const;

    bool matchText(
        TextMatcher* textMatcher,
        const ObjectTrack& track,
        const AbstractObjectTypeDictionary& objectTypeDictionary) const;
};

void serializeToParams(const Filter& filter, QnRequestParamList* params);
bool deserializeFromParams(const QnRequestParamList& params, Filter* filter,
    QnResourcePool* resourcePool);

::std::ostream& operator<<(::std::ostream& os, const Filter& filter);
QString toString(const Filter& filter);

#define Filter_analytics_storage_Fields \
    (deviceIds)(objectTypeId)(objectTrackId)(timePeriod)(boundingBox)(freeText)\
    (maxObjectTracksToSelect)(needFullTrack)(sortOrder)
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json));

//-------------------------------------------------------------------------------------------------

using LookupResult = std::vector<ObjectTrackEx>;

//-------------------------------------------------------------------------------------------------

struct TimePeriodsLookupOptions
{
    /**
     * If distance between two time periods less than this value, then those periods must be merged
     * ignoring the gap.
     */
    std::chrono::milliseconds detailLevel = std::chrono::milliseconds::zero();
};

//-------------------------------------------------------------------------------------------------

enum class ResultCode
{
    ok,
    retryLater,
    error,
};

ResultCode dbResultToResultCode(nx::sql::DBResult dbResult);

nx::network::http::StatusCode::Value toHttpStatusCode(ResultCode resultCode);
ResultCode fromHttpStatusCode(nx::network::http::StatusCode::Value statusCode);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)

} // namespace nx::analytics::db

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::analytics::db::ResultCode), (lexical))
