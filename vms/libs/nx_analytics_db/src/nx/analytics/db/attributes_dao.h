#pragma once

#include <QtCore/QCache>

#include <nx/sql/query_context.h>

#include <analytics/db/analytics_db_types.h>

namespace std { uint qHash(const std::set<int64_t>& value); }

namespace nx::analytics::db {

class AbstractObjectTypeDictionary;

/**
 * Implements object attributes storing/looking up on top of SQLITE.
 * SQLITE fts4 text index is used to provide text search functionality.
 *
 * Text index format:
 * textIndexRecord = [ objectTypeToken " " ] [ attributeTokens ]
 * objectTypeToken = encodeZeros(objectType)
 * attributeTokens = attributeToken *( "000" attributeToken )
 * attributeToken = toZeroEncoding(attributeName) " " encodeZeros(attributeValue)
 *
 * Function "TEXT toZeroEncoding(TEXT)" replaces all non-alphadigit characters of text with
 * "0" HEX(text char).
 * Function "TEXT encodeZeros(TEXT)" replaces "0" found in the input text with "030".
 *
 * NOTE: Not thread-safe.
 */
class NX_ANALYTICS_DB_API AttributesDao
{
public:
    static constexpr char kNameValueSeparator = ' ';
    static constexpr char kParamNamePrefix[] = "000";

    AttributesDao(const AbstractObjectTypeDictionary& objectTypeDictionary);

    /**
     * Saves attributes and makes them available for the full-text search.
     * @param objectTypeId Resolved to object type name and made available through the full-text search.
     * @return attributesId
     * NOTE: Throws on failure.
     */
    int64_t insertOrFetchAttributes(
        nx::sql::QueryContext* queryContext,
        const QString& objectTypeId,
        const std::vector<common::metadata::Attribute>& eventAttributes);

    int64_t combineAttributes(
        nx::sql::QueryContext* queryContext,
        const std::set<int64_t>& attributesIds);

    std::set<int64_t> lookupCombinedAttributes(
        nx::sql::QueryContext* queryContext,
        const QString& text);

    void clear();

    static QByteArray serialize(
        const std::optional<QString>& objectTypeName,
        const std::vector<common::metadata::Attribute>& attributes);

    static std::vector<common::metadata::Attribute> deserialize(
        const QString& attributesStr);

    //---------------------------------------------------------------------------------------------

    static QString buildSearchableText(
        const QString& objectTypeName,
        const std::vector<common::metadata::Attribute>& attributes);

    /**
     * Converts user text search expression into sqlite FTS4 MATCH expression.
     * @param text See Filter::TextMatchContext for user text format description.
     * @return See AttributesDao class description for the text index format.
     */
    static QString convertTextFilterToSqliteFtsExpression(const QString& text);

private:
    QCache<QByteArray /*md5*/, int64_t /*id*/> m_attributesCache;
    QCache<std::set<int64_t> /*attributesIds*/, int64_t /*id*/> m_combinedAttrsCache;
    const AbstractObjectTypeDictionary& m_objectTypeDictionary;

    /**
     * @return Attributes set id.
     */
    int64_t insertAttributes(
        nx::sql::QueryContext* queryContext,
        const std::optional<QString>& objectTypeName,
        const std::vector<common::metadata::Attribute>& attributes,
        const QByteArray& serializedAttributes);

    void addToAttributesCache(int64_t id, const QByteArray& content);
    int64_t findAttributesIdInCache(const QByteArray& content);
    void removeFromAttributesCache(const QByteArray& content);

    int64_t saveToDb(
        nx::sql::QueryContext* queryContext,
        const std::set<int64_t>& attributesIds);

    static QString prepareAttributeTokens(
        const std::vector<common::metadata::Attribute>& attributes);

    static QString toZeroEncoding(const QString& text);
    static bool isLatinLetterOrNumber(const QChar& ch);
    static QString toZeroEncoding(const QChar& ch);

    static QString encodeZeros(const QString& text);
    static QString encodeTextValue(const QString& text);
};

} // namespace nx::analytics::db
