#pragma once

#include <nx/network/buffer.h>

#include <vector>

#include "nx/cloud/aws/xml/serialize.h"
#include "nx/cloud/aws/xml/deserialize.h"

namespace nx::cloud::aws {

namespace s3 {

struct NX_AWS_CLIENT_API ListBucketRequest
{
    std::string delimiter;
    std::string encodingType;
    int maxKeys = 0; //< If left 0, aws uses 1000
    std::string prefix;
    std::string continuationToken;
    bool fetchOwner;
    std::string startAfter;
};

//-------------------------------------------------------------------------------------------------

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
    std::string delimiter;
    bool isTruncated = false;
    std::vector<Contents> contents;
};

} // namespace s3

namespace xml {

template<>
bool deserialize(QXmlStreamReader* xml, s3::ListBucketResult* outObject);

template<>
void serialize(QXmlStreamWriter* xml, const s3::ListBucketResult& object);

} // namespace xml

} // namespace nx::cloud::aws