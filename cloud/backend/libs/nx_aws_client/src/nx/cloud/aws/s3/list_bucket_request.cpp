#include "list_bucket_request.h"

namespace nx::cloud::aws {

namespace {

using namespace xml;

static constexpr char kContents[] = "Contents";

static const Field<s3::Contents>::Parsers kContentsParsers = {
    {"Key", [](auto* obj, auto& value) { return assign(&obj->key, value); }},
    {"LastModified", [](auto* obj, auto& value) { return assign(&obj->lastModified, value); }},
    {"ETag", [](auto* obj, auto& value) { return assign(&obj->etag, value); }},
    {"Size", [](auto* obj, auto& value) { return assign(&obj->size, value); }},
    {"StorageClass", [](auto* obj, auto& value) { return assign(&obj->storageClass, value); }},
};

static const Field<s3::ListBucketResult>::Parsers kListBucketResultParsers = {
    {"Name", [](auto* obj, auto& value) { return assign(&obj->name, value); }},
    {"Prefix", [](auto* obj, auto& value) { return assign(&obj->prefix, value); }},
    {
        "NextContinuationToken",
        [](auto* obj, auto& value) { return assign(&obj->nextContinuationToken, value); }},
    {"KeyCount", [](auto* obj, auto& value) { return assign(&obj->keyCount, value); }},
    {"MaxKeys", [](auto* obj, auto& value) { return assign(&obj->maxKeys, value); }},
    {"Delimiter", [](auto* obj, auto& value) { return assign(&obj->delimiter, value); }},
    {"IsTruncated", [](auto* obj, auto& value) { return assign(&obj->isTruncated, value); }}
};

} // namespace

namespace xml {

template<>
bool deserialize(
    QXmlStreamReader* reader,
    s3::ListBucketResult* outObject)
{
    while (!reader->atEnd())
    {
        if (reader->name() == kContents)
        {
            if (!deserialize(reader, kContentsParsers, kContents, &outObject->contents))
                return false;
            continue;
        }

        if (!parseNextField(reader, kListBucketResultParsers, outObject))
            return false;
    }

    return true;
}

template<>
void serialize(QXmlStreamWriter* writer, const s3::ListBucketResult& object)
{
    writeElement(writer, "Name", object.name);
    writeElement(writer, "Prefix", object.prefix);
    if (!object.nextContinuationToken.empty())
        writeElement(writer, "NextContinuationToken", object.nextContinuationToken);
    writeElement(writer, "KeyCount", object.keyCount);
    writeElement(writer, "MaxKeys", object.maxKeys);
    if (!object.delimiter.empty())
        writeElement(writer, "Delimitter", object.delimiter);
    writeElement(writer, "IsTruncated", object.isTruncated);

    for (const auto& content : object.contents)
    {
        NestedObject element(writer, content);
        writeElement(writer, "Key", content.key);
        writeElement(writer, "LastModified", content.lastModified);
        writeElement(writer, "ETag", content.etag);
        writeElement(writer, "Size", content.size);
        writeElement(writer, "StorageClass", content.storageClass);
    }
}

} // namespace xml

} // namespace nx::cloud::aws
