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

static const Field<sts::AssumeRoleResult>::Assigners kAssumeRoleResultAssigners = {
    {kPackedPolicySize, [](auto* obj, auto& value) { assign(&obj->packedPolicySize, value); }},
};

static const Field<sts::Credentials>::Assigners kCredentialsAssigners = {
    {kAccessKeyId, [](auto* obj, auto& value) { assign(&obj->accessKeyId, value); }},
    {kSecretAccessKey, [](auto* obj, auto& value) { assign(&obj->secretAccessKey, value); }},
    {kSessionToken, [](auto* obj, auto& value) { assign(&obj->sessionToken, value); }},
    {kExpiration, [](auto* obj, auto& value) { assign(&obj->expiration, value); }}
};

static const Field<sts::AssumedRoleUser>::Assigners kAssumedRoleUserAssigners{
    {kAssumedRoleId, [](auto* obj, auto& value) { assign(&obj->assumedRoleId, value); }},
    {kArn, [](auto* obj, auto& value) { assign(&obj->arn, value); }}
};

}

template<>
void serialize(QXmlStreamWriter* writer, const sts::AssumeRoleResult& object)
{
    writeElement(writer, kPackedPolicySize, object.packedPolicySize);
    {
        NestedObject(writer, object.credentials);
        writeElement(writer, kAccessKeyId, object.credentials.accessKeyId);
        writeElement(writer, kSecretAccessKey, object.credentials.secretAccessKey);
        writeElement(writer, kSessionToken, object.credentials.sessionToken);
        writeElement(writer, kExpiration, object.credentials.expiration);
    }
    {
        NestedObject(writer, object.assumedRoleUser);
        writeElement(writer, kAssumedRoleId, object.assumedRoleUser.assumedRoleId);
        writeElement(writer, kArn, object.assumedRoleUser.arn);
    }
}

template<>
bool deserialize(QXmlStreamReader* xml, sts::AssumeRoleResult* outObject)
{
    while (!xml->atEnd())
    {
        if (xml->name() == kCredentials)
        {
            if (!deserialize(xml, kCredentialsAssigners, kCredentials, &outObject->credentials))
                return false;
        }

        if (xml->name() == kAssumedRoleUser)
        {
            if (!deserialize(xml, kAssumedRoleUserAssigners, kCredentials, &outObject->assumedRoleUser))
                return false;
        }

        if (!parseNextField(xml, kAssumeRoleResultAssigners, outObject))
            return false;
    }

    return true;
}

} // namespace nx::cloud::aws::xml
