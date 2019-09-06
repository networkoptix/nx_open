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
    query.add("Action", "AssumeRole").add("Version", "2011-06-15");

    if (!object.roleArn.empty())
        query.add("RoleArn", object.roleArn);
    if (!object.roleSessionName.empty())
        query.add("RoleSessionName", object.roleSessionName);
    int i = 0;
    for (const auto& policyArn : object.policyArns)
    {
        if (!policyArn.empty())
        {
            query.add(
                QString("PolicyArns.member.") + QString::number(++i),
                policyArn);
        }
    }
    if (!object.policy.empty())
        query.add("Policy", object.policy);
    if (object.durationSeconds > 0)
        query.add("DurationSeconds", object.durationSeconds);
    if (!object.externalId.empty())
        query.add("ExternalId", object.externalId);
    if (!object.serialNumber.empty())
        query.add("SerialNumber", object.serialNumber);
    if (!object.tokenCode.empty())
        query.add("TokenCode", object.tokenCode);

    return query;
}

bool deserialize(const nx::utils::UrlQuery& query, AssumeRoleRequest* outObject)
{
    if (!query.hasKey("Action") || !query.hasKey("Version"))
        return false;

    if (!query.valueIfExists("RoleArn", &outObject->roleArn)
        || !query.valueIfExists("RoleSessionName", &outObject->roleSessionName))
    {
        return false;
    }

    int i = 1;
    QString arn(QString("PolicyArns.member.") + QString::number(i));
    while(query.hasKey(arn))
    {
        std::string value;
        query.value(arn, &value);
        outObject->policyArns.emplace_back(std::move(value));
        arn = QString("PolicyArns.member.") + QString::number(++i);
    }
    query.value("Policy", &outObject->policy);
    query.value("DurationSeconds", &outObject->durationSeconds);
    query.value("ExternalId", &outObject->externalId);
    query.value("SerialNumber", &outObject->serialNumber);
    query.value("TokenCode", &outObject->tokenCode);

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
