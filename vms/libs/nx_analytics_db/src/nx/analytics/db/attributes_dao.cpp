#include "attributes_dao.h"

#include <QtCore/QRegularExpression>

#include <nx/fusion/model_functions.h>

#include <analytics/db/config.h>

#include "abstract_object_type_dictionary.h"

namespace std {

uint qHash(const std::set<int64_t>& value)
{
    return std::accumulate(
        value.begin(), value.end(),
        (uint)0,
        [](uint hash, int64_t id) { return hash + ::qHash((long long)id); });
}

} // namespace std

//-------------------------------------------------------------------------------------------------

namespace nx::analytics::db {

static constexpr int kAttributesCacheSize = 10001;

AttributesDao::AttributesDao(AbstractObjectTypeDictionary* objectTypeDictionary):
    m_objectTypeDictionary(objectTypeDictionary)
{
    m_attributesCache.setMaxCost(kAttributesCacheSize);
    m_combinedAttrsCache.setMaxCost(kAttributesCacheSize);
}

int64_t AttributesDao::insertOrFetchAttributes(
    sql::QueryContext* queryContext,
    const QString& objectTypeId,
    const std::vector<common::metadata::Attribute>& attributes)
{
    const auto objectTypeName = m_objectTypeDictionary->idToName(objectTypeId);
    const auto content = serialize(objectTypeName, attributes);

    auto attributesId = findAttributesIdInCache(content);
    if (attributesId >= 0)
        return attributesId;

    auto findIdQuery = queryContext->connection()->createQuery();
    findIdQuery->prepare("SELECT id FROM unique_attributes WHERE content=:content");
    findIdQuery->bindValue(":content", content);
    findIdQuery->exec();
    if (findIdQuery->next())
    {
        const auto id = findIdQuery->value(0).toLongLong();
        addToAttributesCache(id, content);
        return id;
    }

    // No such value. Inserting.
    const auto id = insertAttributes(queryContext, objectTypeName, attributes, content);

    addToAttributesCache(id, content);

    queryContext->transaction()->addOnTransactionCompletionHandler(
        [this, content](nx::sql::DBResult result)
        {
            if (result != nx::sql::DBResult::ok)
                removeFromAttributesCache(content);
        });

    return id;
}

int64_t AttributesDao::combineAttributes(
    nx::sql::QueryContext* queryContext,
    const std::set<int64_t>& attributesIds)
{
    if (attributesIds.empty())
        return -1;

    if (int64_t* id = m_combinedAttrsCache.object(attributesIds);
        id != nullptr)
    {
        return *id;
    }

    auto combinationId = saveToDb(queryContext, attributesIds);

    m_combinedAttrsCache.insert(
        attributesIds,
        std::make_unique<int64_t>(combinationId).release());

    queryContext->transaction()->addOnTransactionCompletionHandler(
        [this, attributesIds](nx::sql::DBResult result)
        {
            if (result != nx::sql::DBResult::ok)
                m_combinedAttrsCache.remove(attributesIds);
        });

    return combinationId;
}

std::set<int64_t> AttributesDao::lookupCombinedAttributes(
    nx::sql::QueryContext* queryContext,
    const QString& text)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        SELECT distinct combination_id
        FROM combined_attributes
        JOIN attributes_text_index on docid = attributes_id
        WHERE content MATCH ?
    )sql");

    const auto filterExpression = convertTextFilterToSqliteFtsExpression(text);
    query->addBindValue(filterExpression);
    query->exec();

    std::set<int64_t> attributesIds;
    while (query->next())
        attributesIds.insert(query->value(0).toLongLong());

    return attributesIds;
}

void AttributesDao::clear()
{
    m_attributesCache.clear();
    m_combinedAttrsCache.clear();
}

static constexpr char kObjectTypeAttributesSeparator = '\n';

QByteArray AttributesDao::serialize(
    const std::optional<QString>& objectTypeName,
    const std::vector<common::metadata::Attribute>& attributes)
{
    QByteArray result;
    if (objectTypeName)
        result += objectTypeName->toUtf8();
    // Assuming that object type does not contain kObjectTypeAttributesSeparator.
    result += kObjectTypeAttributesSeparator;
    result += QnUbjson::serialized(attributes);
    return result;
}

std::vector<common::metadata::Attribute> AttributesDao::deserialize(
    const QString& attributesStr)
{
    const auto data = attributesStr.toUtf8();
    auto pos = data.indexOf(kObjectTypeAttributesSeparator);
    if (pos == -1)
        return QnUbjson::deserialized<std::vector<common::metadata::Attribute>>(data);

    ++pos; // Skipping the separator.
    return QnUbjson::deserialized<std::vector<common::metadata::Attribute>>(
        QByteArray::fromRawData(data.data() + pos, data.size() - pos));
}

QString AttributesDao::convertTextFilterToSqliteFtsExpression(const QString& text)
{
    auto result = text;
    result.replace(
        QRegularExpression("([\\w\\d\\-_]+)\\s*:\\s*([\\w\\d\\-_]+)"),
        kParamNamePrefix + QString("\\1 NEAR/0 \\2"));
    result.replace("$", kParamNamePrefix);
    return result;
}

int64_t AttributesDao::insertAttributes(
    nx::sql::QueryContext* queryContext,
    const std::optional<QString>& objectTypeName,
    const std::vector<common::metadata::Attribute>& attributes,
    const QByteArray& serializedAttributes)
{
    // This table contain full attributes info, which then used to build track.
    auto insertContentQuery = queryContext->connection()->createQuery();
    insertContentQuery->prepare("INSERT INTO unique_attributes(content) VALUES (:content)");
    insertContentQuery->bindValue(":content", serializedAttributes);
    insertContentQuery->exec();
    const auto id = insertContentQuery->impl().lastInsertId().toLongLong();

    // Full text search table stores only attribute values.
    QString contentForTextSearch;

    if (objectTypeName)
        contentForTextSearch += *objectTypeName + kNameValueSeparator;

    for (const auto& attribute: attributes)
    {
        if (!contentForTextSearch.isEmpty())
            contentForTextSearch += kNameValueSeparator;

        // NOTE: SQLITE fts NEAR term does not support token ordering.
        // So, inserting additional separator for the following case:
        // name1 foo name2 yahoo.
        // Search "name2: foo" is not expected to find the string, but it will by default.
        // Modifying string to the following: name1 foo 000 name2 yahoo

        contentForTextSearch += kParamNamePrefix;
        contentForTextSearch += kNameValueSeparator;
        contentForTextSearch += kParamNamePrefix + attribute.name;
        contentForTextSearch += kNameValueSeparator;
        contentForTextSearch += attribute.value;
    }

    insertContentQuery = queryContext->connection()->createQuery();
    insertContentQuery->prepare(
        "INSERT INTO attributes_text_index(docid, content) VALUES (:id, :content)");
    insertContentQuery->bindValue(":id", id);
    insertContentQuery->bindValue(":content", contentForTextSearch);
    insertContentQuery->exec();

    return id;
}

void AttributesDao::addToAttributesCache(
    int64_t id,
    const QByteArray& content)
{
    m_attributesCache.insert(
        QCryptographicHash::hash(content, QCryptographicHash::Md5),
        std::make_unique<int64_t>(id).release());
}

int64_t AttributesDao::findAttributesIdInCache(
    const QByteArray& content)
{
    const auto md5 = QCryptographicHash::hash(content, QCryptographicHash::Md5);

    int64_t* id = m_attributesCache.object(md5);
    if (id == nullptr)
        return -1;
    return *id;
}

void AttributesDao::removeFromAttributesCache(const QByteArray& content)
{
    m_attributesCache.remove(QCryptographicHash::hash(content, QCryptographicHash::Md5));
}

int64_t AttributesDao::saveToDb(
    nx::sql::QueryContext* queryContext,
    const std::set<int64_t>& attributesIds)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        INSERT INTO combined_attributes (combination_id, attributes_id) VALUES (?, ?)
    )sql");

    query->bindValue(0, -1);
    query->bindValue(1, (long long) *attributesIds.begin());
    query->exec();

    const auto combinationId = query->lastInsertId().toLongLong();

    // Replacing -1 with the correct value.
    queryContext->connection()->executeQuery(
        "UPDATE combined_attributes SET combination_id = ? WHERE rowid = ?",
        combinationId, combinationId);

    for (auto it = std::next(attributesIds.begin()); it != attributesIds.end(); ++it)
    {
        query->bindValue(0, combinationId);
        query->bindValue(1, (long long) *it);
        query->exec();
    }

    return combinationId;
}

} // namespace nx::analytics::db
