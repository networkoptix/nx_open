#include "attributes_dao.h"

#include <nx/fusion/model_functions.h>

#include <analytics/db/config.h>

namespace nx::analytics::db {

AttributesDao::AttributesDao()
{
    static constexpr int kCacheSize = 101;

    m_attributesCache.setMaxCost(kCacheSize);
}

int64_t AttributesDao::insertOrFetchAttributes(
    sql::QueryContext* queryContext,
    const std::vector<common::metadata::Attribute>& eventAttributes)
{
    const auto content = kUseTrackAggregation
        ? QnUbjson::serialized(eventAttributes)
        : QJson::serialized(eventAttributes);

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
    auto insertContentQuery = queryContext->connection()->createQuery();
    insertContentQuery->prepare("INSERT INTO unique_attributes(content) VALUES (:content)");
    insertContentQuery->bindValue(":content", content);
    insertContentQuery->exec();
    const auto id = insertContentQuery->impl().lastInsertId().toLongLong();
    addToAttributesCache(id, content);

    // NOTE: Following string is in non-reversable format. So, cannot use it to store attributes.
    // But, cannot use JSON for full text search since it contains additional information.
    const auto contentForTextSearch =
        containerString(eventAttributes, lit("; ") /*delimiter*/,
            QString() /*prefix*/, QString() /*suffix*/, QString() /*empty*/);

    insertContentQuery = queryContext->connection()->createQuery();
    insertContentQuery->prepare(
        "INSERT INTO attributes_text_index(docid, content) VALUES (:id, :content)");
    insertContentQuery->bindValue(":id", id);
    insertContentQuery->bindValue(":content", contentForTextSearch);
    insertContentQuery->exec();

    return id;
}

void AttributesDao::clear()
{
    m_attributesCache.clear();
}

std::vector<common::metadata::Attribute> AttributesDao::deserialize(
    const QString& attributesStr)
{
    return kUseTrackAggregation
        ? QnUbjson::deserialized<std::vector<common::metadata::Attribute>>(
            attributesStr.toUtf8())
        : QJson::deserialized<std::vector<common::metadata::Attribute>>(
            attributesStr.toUtf8());
}

void AttributesDao::addToAttributesCache(
    int64_t id,
    const QByteArray& content)
{
    auto cachedValue = std::make_unique<int64_t>(id);
    if (m_attributesCache.insert(
            QCryptographicHash::hash(content, QCryptographicHash::Md5),
            cachedValue.get()))
    {
        cachedValue.release();
    }
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

} // namespace nx::analytics::db
