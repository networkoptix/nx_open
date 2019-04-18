#include "attributes_dao.h"

#include <nx/fusion/model_functions.h>

namespace nx::analytics::storage {

long long AttributesDao::insertOrFetchAttributes(
    sql::QueryContext* queryContext,
    const std::vector<common::metadata::Attribute>& eventAttributes)
{
    const auto content = QJson::serialized(eventAttributes);

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

std::vector<common::metadata::Attribute> AttributesDao::deserialize(
    const QString& attributesStr)
{
    return QJson::deserialized<std::vector<common::metadata::Attribute>>(
        attributesStr.toUtf8());
}

void AttributesDao::addToAttributesCache(
    long long id,
    const QByteArray& content)
{
    static constexpr int kCacheSize = 101;

    m_attributesCache.push_back({
        QCryptographicHash::hash(content, QCryptographicHash::Md5),
        id });

    if (m_attributesCache.size() > kCacheSize)
        m_attributesCache.pop_front();
}

long long AttributesDao::findAttributesIdInCache(
    const QByteArray& content)
{
    const auto md5 = QCryptographicHash::hash(content, QCryptographicHash::Md5);

    const auto it = std::find_if(
        m_attributesCache.rbegin(), m_attributesCache.rend(),
        [&md5](const auto& entry) { return entry.md5 == md5; });

    return it != m_attributesCache.rend() ? it->id : -1;
}

} // namespace nx::analytics::storage
