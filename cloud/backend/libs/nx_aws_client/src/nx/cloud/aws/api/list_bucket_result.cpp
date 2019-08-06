#include "list_bucket_result.h"

#include "xml_parse_helper.h"

namespace nx::cloud::aws::api {

namespace {

static constexpr char kListBucketResult[] = "ListBucketResult";
static constexpr char kContents[] = "Contents";
static constexpr char kCommonPrefixes[] = "CommonPrefixes";

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

static const xml::Field<CommonPrefixes>::Assigners kCommonPrefixesAssigners = {
    {"Prefix", [](auto* obj, auto& value) { obj->prefix = value.toStdString(); }}
};

} // namespace

namespace xml {

NX_AWS_CLIENT_API bool deserialize(
    const nx::Buffer& data,
    ListBucketResult* outObject)
{
    QXmlStreamReader xml(data);

    while (!xml.atEnd())
    {
        if (xml.name() == kContents)
        {
            if (!deserialize(&xml, kContentsAssigners, kContents, &outObject->contents))
                return false;
            continue;
        }

        if (xml.name() == kCommonPrefixes)
        {
            if (!deserialize(&xml, kCommonPrefixesAssigners, kCommonPrefixes, &outObject->commonPrefixes))
                return false;
            continue;
        }

        if (!parseNextField(&xml, kListBucketResultAssigners, outObject))
            return false;
    }

    return true;
}

} // namespace xml
} // namespace nx::cloud::aws::api
