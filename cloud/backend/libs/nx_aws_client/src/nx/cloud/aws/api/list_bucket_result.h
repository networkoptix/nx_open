#pragma once

#include <nx/network/buffer.h>

#include <vector>

#include "xml/serialize.h"
#include "xml/deserialize.h"

namespace nx::cloud::aws::api {

struct NX_AWS_CLIENT_API Contents
{
    std::string key;
    std::string lastModified;
    std::string etag;
    int size = 0;
    std::string storageClass;
};

//-------------------------------------------------------------------------------------------------

struct NX_AWS_CLIENT_API ListBucketResult
{
    std::string name;
    std::string prefix;
    std::string nextContinuationToken;
    int keyCount = 0;
    int maxKeys = 0;
    std::string delimitter;
    bool isTruncated = false;
    std::vector<Contents> contents;
};

namespace xml {

template<>
NX_AWS_CLIENT_API bool deserialize(QXmlStreamReader* xml, ListBucketResult* outObject);

template<>
NX_AWS_CLIENT_API void serialize(QXmlStreamWriter* xml, const ListBucketResult& object);

} // namespace xml

} // namespace nx::cloud::aws::api