#pragma once

#include <QtCore/QUrlQuery>

#include <string>
#include <vector>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/db/api/system_data.h>

namespace nx::cloud::db::api {

#define SystemRegistrationData_Fields (name)(customization)(opaque)

// TODO: #ak Add corresponding parser/serializer to fusion and remove this function.
bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const systemData);
void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery);


// TODO: #ak Add corresponding parser/serializer to fusion and remove this function.
//bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemData* const systemData );

#define SystemData_Fields (id)(name)(customization)(authKey)(ownerAccountEmail) \
                          (status)(cloudConnectionSubscriptionStatus)(systemSequence) \
                          (opaque)(registrationTime)
#define SystemDataList_Fields (systems)

/**
 * For requests passing just system id.
 */
class SystemId
{
public:
    std::string systemId;

    SystemId();
    SystemId(std::string systemIdStr);
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemId* const systemId);
void serializeToUrlQuery(const SystemId& data, QUrlQuery* const urlQuery);

#define SystemId_Fields (systemId)

//-------------------------------------------------------------------------------------------------
// Filter

void serializeToUrlQuery(const Filter& data, QUrlQuery* const urlQuery);
void serialize(QnJsonContext* ctx, const Filter& filter, QJsonValue* target);


//-------------------------------------------------------------------------------------------------
// SystemAttributesUpdate

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemAttributesUpdate* const data);
void serializeToUrlQuery(const SystemAttributesUpdate& data, QUrlQuery* const urlQuery);

void serialize(QnJsonContext*, const SystemAttributesUpdate&, QJsonValue*);
bool deserialize(QnJsonContext*, const QJsonValue&, SystemAttributesUpdate*);

//-------------------------------------------------------------------------------------------------
// SystemHealthHistory

#define SystemHealthHistoryItem_Fields (timestamp)(state)
#define SystemHealthHistory_Fields (events)

//-------------------------------------------------------------------------------------------------
// System sharing data

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharing* const systemSharing);
void serializeToUrlQuery(const SystemSharing& data, QUrlQuery* const urlQuery);

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharingList* const systemSharing);

#define SystemSharing_Fields (accountEmail)(systemId)(accessRole)(userRoleId)(customPermissions)(isEnabled)(vmsUserId)
#define SystemSharingList_Fields (sharing)

#define SystemSharingEx_Fields SystemSharing_Fields(accountId)(accountFullName)(usageFrequency)(lastLoginTime)
#define SystemSharingExList_Fields (sharing)

#define SystemAccessRoleData_Fields (accessRole)
#define SystemAccessRoleList_Fields (accessRoles)

#define SystemMergeInfo_Fields (role)(startTime)(anotherSystemId)

#define SystemDataEx_Fields SystemData_Fields \
    (ownerFullName)(accessRole)(sharingPermissions)(stateOfHealth) \
    (usageFrequency)(lastLoginTime)(mergeInfo)(capabilities)
#define SystemDataExList_Fields (systems)


//-------------------------------------------------------------------------------------------------
// UserSessionDescriptor

bool loadFromUrlQuery(const QUrlQuery& urlQuery, UserSessionDescriptor* const data);
void serializeToUrlQuery(const UserSessionDescriptor&, QUrlQuery* const urlQuery);

void serialize(QnJsonContext*, const UserSessionDescriptor&, QJsonValue*);
bool deserialize(QnJsonContext*, const QJsonValue&, UserSessionDescriptor*);

//-------------------------------------------------------------------------------------------------
// common functions

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemId)(SystemAttributesUpdate),
    (json));

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemMergeInfo)(SystemDataEx)(SystemDataList)(SystemDataExList)(SystemSharingList) \
        (SystemSharingEx)(SystemSharingExList)(SystemAccessRoleData)(SystemAccessRoleList) \
        (SystemHealthHistoryItem)(SystemHealthHistory),
    (json));

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cloud::db::api::SystemStatus)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cloud::db::api::SystemHealth)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cloud::db::api::SystemAccessRole)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cloud::db::api::FilterField)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cloud::db::api::MergeRole)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cloud::db::api::SystemCapabilityFlag)

} // namespace nx::cloud::db::api

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cloud::db::api::SystemStatus), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cloud::db::api::SystemHealth), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cloud::db::api::SystemAccessRole), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cloud::db::api::FilterField), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cloud::db::api::MergeRole), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cloud::db::api::SystemCapabilityFlag), (lexical))
