// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrlQuery>

#include <string>
#include <vector>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/db/api/system_data.h>

namespace nx::cloud::db::api {

#define SystemRegistrationData_Fields (name)(customization)(opaque)

NX_REFLECTION_INSTRUMENT(SystemRegistrationData, SystemRegistrationData_Fields)

// TODO: #akolesnikov Add corresponding parser/serializer to fusion and remove this function.
bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const systemData);
void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery);

NX_REFLECTION_INSTRUMENT_ENUM(SystemStatus,
    invalid,
    notActivated,
    activated,
    deleted_,
    beingMerged,
    deletedByMerge
)

// TODO: #akolesnikov Add corresponding parser/serializer to fusion and remove this function.
//bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemData* const systemData );

#define SystemData_Fields (id)(name)(customization)(authKey)(authKeyHash)(ownerAccountEmail) \
                          (status)(cloudConnectionSubscriptionStatus)(systemSequence) \
                          (opaque)(registrationTime)(system2faEnabled)

NX_REFLECTION_INSTRUMENT(SystemData, SystemData_Fields)

#define SystemDataList_Fields (systems)

NX_REFLECTION_INSTRUMENT(SystemDataList, SystemDataList_Fields)

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

NX_REFLECTION_INSTRUMENT(SystemId, SystemId_Fields)

//-------------------------------------------------------------------------------------------------
// SystemIdList

void serializeToUrlQuery(const SystemIdList& data, QUrlQuery* const urlQuery);

#define SystemIdList_Fields (systemIds)

NX_REFLECTION_INSTRUMENT(SystemIdList, SystemIdList_Fields)

//-------------------------------------------------------------------------------------------------
// Filter

NX_REFLECTION_INSTRUMENT_ENUM(FilterField, customization, systemStatus)

void serializeToUrlQuery(const Filter& data, QUrlQuery* const urlQuery);
void serialize(QnJsonContext* ctx, const Filter& filter, QJsonValue* target);

//-------------------------------------------------------------------------------------------------
// SystemAttributesUpdate

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemAttributesUpdate* const data);
void serializeToUrlQuery(const SystemAttributesUpdate& data, QUrlQuery* const urlQuery);

void serialize(QnJsonContext*, const SystemAttributesUpdate&, QJsonValue*);
bool deserialize(QnJsonContext*, const QJsonValue&, SystemAttributesUpdate*);

#define SystemAttributesUpdate_Fields (systemId)(name)(opaque)(system2faEnabled)(totp)(mfaCode)

NX_REFLECTION_INSTRUMENT(SystemAttributesUpdate, SystemAttributesUpdate_Fields)

//-------------------------------------------------------------------------------------------------
// SystemHealthHistory

#define SystemHealthHistoryItem_Fields (timestamp)(state)

NX_REFLECTION_INSTRUMENT(SystemHealthHistoryItem, SystemHealthHistoryItem_Fields)

#define SystemHealthHistory_Fields (events)

NX_REFLECTION_INSTRUMENT(SystemHealthHistory, SystemHealthHistory_Fields)

//-------------------------------------------------------------------------------------------------

#define MergeRequest_Fields (systemId)(masterSystemAccessToken)(slaveSystemAccessToken)

NX_REFLECTION_INSTRUMENT(MergeRequest, MergeRequest_Fields)

//-------------------------------------------------------------------------------------------------
// System sharing data

NX_REFLECTION_INSTRUMENT_ENUM(SystemAccessRole,
    none,
    disabled,
    custom,
    liveViewer,
    viewer,
    advancedViewer,
    localAdmin,
    cloudAdmin,
    maintenance,
    owner,
    system
)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharing* const systemSharing);
void serializeToUrlQuery(const SystemSharing& data, QUrlQuery* const urlQuery);

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharingList* const systemSharing);

#define SystemSharing_Fields (accountEmail)(systemId)(accessRole)(userRoleId)(customPermissions)(isEnabled)(vmsUserId)

NX_REFLECTION_INSTRUMENT(SystemSharing, SystemSharing_Fields)

#define SystemSharingList_Fields (sharing)

NX_REFLECTION_INSTRUMENT(SystemSharingList, SystemSharingList_Fields)

#define SystemSharingEx_Fields SystemSharing_Fields(accountId)(accountFullName)(usageFrequency)(lastLoginTime)

NX_REFLECTION_INSTRUMENT(SystemSharingEx, SystemSharingEx_Fields)

#define SystemSharingExList_Fields (sharing)

NX_REFLECTION_INSTRUMENT(SystemSharingExList, SystemSharingExList_Fields)

#define SystemAccessRoleData_Fields (accessRole)

NX_REFLECTION_INSTRUMENT(SystemAccessRoleData, SystemAccessRoleData_Fields)

#define SystemAccessRoleList_Fields (accessRoles)

NX_REFLECTION_INSTRUMENT(SystemAccessRoleList, SystemAccessRoleList_Fields)

NX_REFLECTION_INSTRUMENT_ENUM(SystemHealth, offline, online, incompatible)

NX_REFLECTION_INSTRUMENT_ENUM(MergeRole, none, master, slave)

#define SystemMergeInfo_Fields (role)(startTime)(anotherSystemId)

NX_REFLECTION_INSTRUMENT(SystemMergeInfo, SystemMergeInfo_Fields)

#define SystemDataEx_Fields SystemData_Fields \
    (ownerFullName)(accessRole)(sharingPermissions)(stateOfHealth) \
    (usageFrequency)(lastLoginTime)(mergeInfo)(capabilities)(version)

NX_REFLECTION_INSTRUMENT(SystemDataEx, SystemDataEx_Fields)

#define SystemDataExList_Fields (systems)

NX_REFLECTION_INSTRUMENT(SystemDataExList, SystemDataExList_Fields)

//-------------------------------------------------------------------------------------------------
// UserSessionDescriptor

bool loadFromUrlQuery(const QUrlQuery& urlQuery, UserSessionDescriptor* const data);
void serializeToUrlQuery(const UserSessionDescriptor&, QUrlQuery* const urlQuery);

void serialize(QnJsonContext*, const UserSessionDescriptor&, QJsonValue*);
bool deserialize(QnJsonContext*, const QJsonValue&, UserSessionDescriptor*);

#define UserSessionDescriptor_Fields (accountEmail)(systemId)

NX_REFLECTION_INSTRUMENT(UserSessionDescriptor, UserSessionDescriptor_Fields)

#define ValidateMSSignatureRequest_Fields (message)(signature)

NX_REFLECTION_INSTRUMENT(ValidateMSSignatureRequest, ValidateMSSignatureRequest_Fields)

//-------------------------------------------------------------------------------------------------
// GetSystemUsersRequest

void serializeToUrlQuery(const GetSystemUsersRequest& data, QUrlQuery* const urlQuery);

#define GetSystemUsersRequest_Fields (systemId)(localOnly)

NX_REFLECTION_INSTRUMENT(GetSystemUsersRequest, GetSystemUsersRequest_Fields)

//-------------------------------------------------------------------------------------------------
// common functions

QN_FUSION_DECLARE_FUNCTIONS(SystemRegistrationData, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemData, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemSharing, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemId, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemIdList, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemAttributesUpdate, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemMergeInfo, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemDataEx, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemDataList, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemDataExList, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemSharingList, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemSharingEx, (ubjson)(json))
QN_FUSION_DECLARE_FUNCTIONS(SystemSharingExList, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemAccessRoleData, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemAccessRoleList, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemHealthHistoryItem, (json))
QN_FUSION_DECLARE_FUNCTIONS(SystemHealthHistory, (json))
QN_FUSION_DECLARE_FUNCTIONS(MergeRequest, (json))
QN_FUSION_DECLARE_FUNCTIONS(ValidateMSSignatureRequest, (json))
QN_FUSION_DECLARE_FUNCTIONS(GetSystemUsersRequest, (json))

//-------------------------------------------------------------------------------------------------

#define CreateSystemOfferRequest_Fields (toAccount)(systemId)(comment)

NX_REFLECTION_INSTRUMENT(CreateSystemOfferRequest, CreateSystemOfferRequest_Fields)
QN_FUSION_DECLARE_FUNCTIONS(CreateSystemOfferRequest, (json))

NX_REFLECTION_INSTRUMENT_ENUM(OfferStatus, offered, accepted, rejected)

#define SystemOffer_Fields (fromAccount)(toAccount)(systemId)(systemName)(comment)(status)

NX_REFLECTION_INSTRUMENT(SystemOffer, SystemOffer_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SystemOffer, (json))

//-------------------------------------------------------------------------------------------------

#define SystemOfferPatch_Fields (comment)(status)

NX_REFLECTION_INSTRUMENT(SystemOfferPatch, SystemOfferPatch_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SystemOfferPatch, (json))

} // namespace nx::cloud::db::api
