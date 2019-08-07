#include "list_bucket_result.h"

namespace nx::cloud::aws::api {

namespace {

static const QString kContents = "Contents";

static const xml::Field<Contents>::Assigners kContentsAssigners = {
    {"Key", [](auto* obj, auto& value) { obj->key = value.toStdString(); }},
    {"LastModified", [](auto* obj, auto& value) { obj->lastModified = value.toStdString(); }},
    {"ETag", [](auto* obj, auto& value) { obj->etag = value.toStdString(); }},
    {"Size", [](auto* obj, auto& value) { obj->size = value.toInt(); }},
    {"StorageClass", [](auto* obj, auto& value) { obj->storageClass = value.toStdString(); }},
};

static const xml::Field<ListBucketResult>::Assigners kListBucketResultAssigners = {
    {"Name", [](auto* obj, auto& value) { obj->name = value.toStdString(); }},
    {"Prefix", [](auto* obj, auto& value) { obj->prefix = value.toStdString(); }},
    {
        "NextContinuationToken",
        [](auto* obj, auto& value) { obj->nextContinuationToken = value.toStdString(); }},
    {"KeyCount", [](auto* obj, auto& value) { obj->keyCount = value.toInt(); }},
    {"MaxKeys", [](auto* obj, auto& value) { obj->maxKeys = value.toInt(); }},
    {"Delimitter", [](auto* obj, auto& value) { obj->delimitter = value.toStdString(); }},
    {"IsTruncated", [](auto* obj, auto& value) { obj->isTruncated = xml::toBool(value); }}
};

} // namespace

namespace xml {

template<>
NX_AWS_CLIENT_API bool deserialize(
    QXmlStreamReader* xml,
    ListBucketResult* outObject)
{
    while (!xml->atEnd())
    {
        if (xml->name() == kContents)
        {
            if (!deserialize(xml, kContentsAssigners, kContents, &outObject->contents))
                return false;
            continue;
        }

        if (!parseNextField(xml, kListBucketResultAssigners, outObject))
            return false;
    }

    return true;
}

template<>
void serialize(QXmlStreamWriter* xml, const ListBucketResult& object)
{
    writeElement(xml, "Name", object.name);
    writeElement(xml, "Prefix", object.prefix);
    if (!object.nextContinuationToken.empty())
        writeElement(xml, "NextContinuationToken", object.nextContinuationToken);
    writeElement(xml, "KeyCount", object.keyCount);
    writeElement(xml, "MaxKeys", object.maxKeys);
    if (!object.delimitter.empty())
        writeElement(xml, "Delimitter", object.delimitter);
    writeElement(xml, "IsTruncated", object.isTruncated);

    for (const auto& content : object.contents)
    {
        ScopedElement element(xml, content);
        writeElement(xml, "Key", content.key);
        writeElement(xml, "LastModified", content.lastModified);
        writeElement(xml, "ETag", content.etag);
        writeElement(xml, "Size", content.size);
        writeElement(xml, "StorageClass", content.storageClass);
    }
}

} // namespace xml
} // namespace nx::cloud::aws::api
