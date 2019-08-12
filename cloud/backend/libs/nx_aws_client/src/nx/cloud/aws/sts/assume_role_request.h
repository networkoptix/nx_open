#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QDateTime>

#include "nx/cloud/aws/xml/serialize.h"
#include "nx/cloud/aws/xml/deserialize.h"

namespace nx::cloud::aws {

namespace sts {

struct NX_AWS_CLIENT_API AssumeRoleRequest
{
    // Required
    std::string roleArn;
    // Required
    std::string roleSessionName;
    std::vector<std::string> policyArns;
    std::string policy;
    int durationSeconds = 0;
    std::string externalId;
    std::string serialNumber;
    std::string tokenCode;
};

struct NX_AWS_CLIENT_API Credentials
{
    std::string accessKeyId;
    std::string secretAccessKey;
    std::string sessionToken;
    std::string expiration;
};

struct NX_AWS_CLIENT_API AssumedRoleUser
{
    std::string assumedRoleId;
    std::string arn;
};

struct NX_AWS_CLIENT_API AssumeRoleResult
{
    Credentials credentials;
    AssumedRoleUser assumedRoleUser;
    int packedPolicySize = 0;
};

} // namespace sts

namespace xml {

template<>
void serialize(QXmlStreamWriter* writer, const sts::AssumeRoleResult& object);

template<>
bool deserialize(QXmlStreamReader* reader, sts::AssumeRoleResult* outObject);

} // namespace xml

} // namespace nx::cloud::aws