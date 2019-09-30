#include "assume_role_request.h"

namespace nx::cloud::aws {

namespace {

using namespace nx::cloud::aws::xml;

static constexpr char kCredentials[] = "Credentials";
static constexpr char kAssumedRoleUser[] = "AssumedRoleUser";

// AssumeRoleResult
static constexpr char kPackedPolicySize[] = "PackedPolicySize";

// Credentials
static constexpr char kSessionToken[] = "SessionToken";
static constexpr char kSecretAccessKey[] = "SecretAccessKey";
static constexpr char kExpiration[] = "Expiration";
static constexpr char kAccessKeyId[] = "AccessKeyId";

// AssumedRoleUser
static constexpr char kAssumedRoleId[] = "AssumedRoleId";
static constexpr char kArn[] = "Arn";

static constexpr char kAction[] = "Action";
static constexpr char kVersion[] = "Version";
static constexpr char kRoleArn[] = "RoleArn";
static constexpr char kRoleSessionName[] = "RoleSessionName";
static constexpr char kPolicyArns[] = "PolicyArns.member.";
static constexpr char kPolicy[] = "Policy";
static constexpr char kDurationSeconds[] = "DurationSeconds";
static constexpr char kExternalId[] = "ExternalId";
static constexpr char kSerialNumber[] = "SerialNumber";
static constexpr char kTokenCode[] = "TokenCode";

static const Field<sts::AssumeRoleResult>::Parsers kAssumeRoleResultParsers = {
    {kPackedPolicySize, [](auto* obj, auto& value) { return assign(&obj->packedPolicySize, value); }},
};

static const Field<sts::Credentials>::Parsers kCredentialsParsers = {
    {kAccessKeyId, [](auto* obj, auto& value) { return assign(&obj->accessKeyId, value); }},
    {kSecretAccessKey, [](auto* obj, auto& value) { return assign(&obj->secretAccessKey, value); }},
    {kSessionToken, [](auto* obj, auto& value) { return assign(&obj->sessionToken, value); }},
    {kExpiration, [](auto* obj, auto& value) { return assign(&obj->expiration, value); }}
};

static const Field<sts::AssumedRoleUser>::Parsers kAssumedRoleUserParsers{
    {kAssumedRoleId, [](auto* obj, auto& value) { return assign(&obj->assumedRoleId, value); }},
    {kArn, [](auto* obj, auto& value) { return assign(&obj->arn, value); }}
};

} // namespace


namespace sts {

nx::utils::UrlQuery serialize(const AssumeRoleRequest& object)
{
    nx::utils::UrlQuery query;
    query.addQueryItem(kAction, "AssumeRole").addQueryItem(kVersion, "2011-06-15");

    if (!object.roleArn.empty())
        query.addQueryItem(kRoleArn, object.roleArn);
    if (!object.roleSessionName.empty())
        query.addQueryItem(kRoleSessionName, object.roleSessionName);
    int i = 0;
    for (const auto& policyArn : object.policyArns)
    {
        if (!policyArn.empty())
            query.addQueryItem(kPolicyArns + QString::number(++i), policyArn);
    }
    if (!object.policy.empty())
        query.addQueryItem(kPolicy, object.policy);
    if (object.durationSeconds > 0)
        query.addQueryItem(kDurationSeconds, object.durationSeconds);
    if (!object.externalId.empty())
        query.addQueryItem(kExternalId, object.externalId);
    if (!object.serialNumber.empty())
        query.addQueryItem(kSerialNumber, object.serialNumber);
    if (!object.tokenCode.empty())
        query.addQueryItem(kTokenCode, object.tokenCode);

    return query;
}

bool deserialize(const nx::utils::UrlQuery& query, AssumeRoleRequest* outObject)
{
    if (!(query.hasQueryItem(kAction) && query.hasQueryItem(kVersion)
        &&query.hasQueryItem(kRoleArn) && query.hasQueryItem(kRoleSessionName)))
    {
        return false;
    }

    outObject->roleArn = query.queryItemValue<std::string>(kRoleArn);
    outObject->roleSessionName = query.queryItemValue<std::string>(kRoleSessionName);

    int i = 1;
    QString arn = kPolicyArns + QString::number(i);
    while(query.hasQueryItem(arn))
    {
        outObject->policyArns.emplace_back(query.queryItemValue<std::string>(arn));
        arn = kPolicyArns + QString::number(++i);
    }
    outObject->policy = query.queryItemValue<std::string>(kPolicy);
    outObject->durationSeconds = query.queryItemValue<int>(kDurationSeconds);
    outObject->externalId = query.queryItemValue<std::string>(kExternalId);
    outObject->tokenCode = query.queryItemValue<std::string>(kTokenCode);

    return true;
}

} // namespace sts

namespace xml {

template<>
void serialize(QXmlStreamWriter* writer, const sts::AssumeRoleResult& object)
{
    writeElement(writer, kPackedPolicySize, object.packedPolicySize);
    {
        NestedObject credentials(writer, object.credentials);
        writeElement(writer, kAccessKeyId, object.credentials.accessKeyId);
        writeElement(writer, kSecretAccessKey, object.credentials.secretAccessKey);
        writeElement(writer, kSessionToken, object.credentials.sessionToken);
        writeElement(writer, kExpiration, object.credentials.expiration);
    }
    {
        NestedObject assumedRoleUser(writer, object.assumedRoleUser);
        writeElement(writer, kAssumedRoleId, object.assumedRoleUser.assumedRoleId);
        writeElement(writer, kArn, object.assumedRoleUser.arn);
    }
}

template<>
bool deserialize(QXmlStreamReader* reader, sts::AssumeRoleResult* outObject)
{
    while (!reader->atEnd())
    {
        if (reader->name() == kCredentials)
        {
            if (!deserialize(reader, kCredentialsParsers, kCredentials, &outObject->credentials))
                return false;
        }

        if (reader->name() == kAssumedRoleUser)
        {
            if (!deserialize(reader, kAssumedRoleUserParsers, kAssumedRoleUser, &outObject->assumedRoleUser))
                return false;
        }

        if (!parseNextField(reader, kAssumeRoleResultParsers, outObject))
            return false;
    }

    return true;
}

} // namespace xml
} // namespace nx::cloud::aws
