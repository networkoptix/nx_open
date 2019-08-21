#include "assume_role_request.h"

namespace nx::cloud::aws::xml {

namespace {

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

template<>
void serialize(QXmlStreamWriter* writer, const sts::AssumeRoleResult& object)
{
    writeElement(writer, kPackedPolicySize, object.packedPolicySize);
    {
        NestedObject<decltype(object.credentials)>(writer, object.credentials);
        writeElement(writer, kAccessKeyId, object.credentials.accessKeyId);
        writeElement(writer, kSecretAccessKey, object.credentials.secretAccessKey);
        writeElement(writer, kSessionToken, object.credentials.sessionToken);
        writeElement(writer, kExpiration, object.credentials.expiration);
    }
    {
        NestedObject<decltype(object.assumedRoleUser)>(writer, object.assumedRoleUser);
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
            if (!deserialize(reader, kAssumedRoleUserParsers, kCredentials, &outObject->assumedRoleUser))
                return false;
        }

        if (!parseNextField(reader, kAssumeRoleResultParsers, outObject))
            return false;
    }

    return true;
}

} // namespace nx::cloud::aws::xml
