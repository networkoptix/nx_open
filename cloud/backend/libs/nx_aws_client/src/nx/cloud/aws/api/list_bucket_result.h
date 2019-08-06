#pragma once

#include <nx/network/buffer.h>

#include <vector>

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

struct NX_AWS_CLIENT_API CommonPrefixes
{
    std::string prefix;
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
    std::vector<CommonPrefixes> commonPrefixes;
};

namespace xml {

NX_AWS_CLIENT_API bool deserialize(const nx::Buffer& data, ListBucketResult* outObject);

} // namespace xml

} // namespace nx::cloud::aws::api