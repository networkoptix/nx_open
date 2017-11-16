#pragma once

#include <QtCore/QUrlQuery>

#include <string>
#include <vector>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/cdb/api/system_data.h>

namespace nx {
namespace cdb {
namespace api {

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

#define SystemDataEx_Fields SystemData_Fields(ownerFullName)(accessRole)(sharingPermissions)(stateOfHealth)(usageFrequency)(lastLoginTime)
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
    (SystemDataEx)(SystemDataList)(SystemDataExList)(SystemSharingList)(SystemSharingEx) \
        (SystemSharingExList)(SystemAccessRoleData)(SystemAccessRoleList) \
        (SystemHealthHistoryItem)(SystemHealthHistory),
    (json));

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cdb::api::SystemStatus)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cdb::api::SystemHealth)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cdb::api::SystemAccessRole)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cdb::api::FilterField)

} // namespace api
} // namespace cdb
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::api::SystemStatus), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::api::SystemHealth), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::api::SystemAccessRole), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::api::FilterField), (lexical))
