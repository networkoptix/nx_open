// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <QtCore/QUrlQuery>

#include <nx/cloud/db/api/system_data.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/uuid.h>

namespace nx::cloud::db::api {

#define SystemRegistrationData_Fields (id)(name)(customization)(opaque)
NX_REFLECTION_INSTRUMENT(SystemRegistrationData, SystemRegistrationData_Fields)

// TODO: #akolesnikov Replace this and similar functions with nx::reflect::urlencoded.
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

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const Filter& value);

//-------------------------------------------------------------------------------------------------
// SystemAttributesUpdate

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemAttributesUpdate* const data);
void serializeToUrlQuery(const SystemAttributesUpdate& data, QUrlQuery* const urlQuery);

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
    custom,
    liveViewer,
    viewer,
    advancedViewer,
    localAdmin,
    cloudAdmin,
    owner,
    system
)

#define ShareSystemRequestV1_Fields (accountEmail)(systemId)(accessRole)(userRoleId) \
    (customPermissions)(isEnabled)(vmsUserId)

NX_REFLECTION_INSTRUMENT(ShareSystemRequestV1, ShareSystemRequestV1_Fields)

#define SystemSharingV1_Fields ShareSystemRequestV1_Fields(accountId)(accountFullName) \
    (usageFrequency)(lastLoginTime)

NX_REFLECTION_INSTRUMENT(SystemSharingV1, SystemSharingV1_Fields)

#define SystemSharingV1List_Fields (sharing)

NX_REFLECTION_INSTRUMENT(SystemSharingV1List, SystemSharingV1List_Fields)

#define SystemAccessRoleData_Fields (accessRole)

NX_REFLECTION_INSTRUMENT(ShareSystemQuery, (sendNotification))

NX_REFLECTION_INSTRUMENT(SystemAccessRoleData, SystemAccessRoleData_Fields)

#define SystemAccessRoleList_Fields (accessRoles)

NX_REFLECTION_INSTRUMENT(SystemAccessRoleList, SystemAccessRoleList_Fields)

NX_REFLECTION_INSTRUMENT_ENUM(SystemHealth, offline, online, incompatible)

NX_REFLECTION_INSTRUMENT_ENUM(MergeRole, none, master, slave)

#define SystemMergeInfo_Fields (role)(startTime)(anotherSystemId)

NX_REFLECTION_INSTRUMENT(SystemMergeInfo, SystemMergeInfo_Fields)

//-------------------------------------------------------------------------------------------------
// UserSessionDescriptor

bool loadFromUrlQuery(const QUrlQuery& urlQuery, UserSessionDescriptor* const data);
void serializeToUrlQuery(const UserSessionDescriptor&, QUrlQuery* const urlQuery);

#define UserSessionDescriptor_Fields (accountEmail)(systemId)

NX_REFLECTION_INSTRUMENT(UserSessionDescriptor, UserSessionDescriptor_Fields)

#define ValidateMSSignatureRequest_Fields (message)(signature)

NX_REFLECTION_INSTRUMENT(ValidateMSSignatureRequest, ValidateMSSignatureRequest_Fields)

//-------------------------------------------------------------------------------------------------
// GetSystemUsersRequest

void serializeToUrlQuery(const GetSystemUsersRequest& data, QUrlQuery* const urlQuery);

NX_REFLECTION_INSTRUMENT(GetSystemUsersRequest, (systemId)(localOnly))

//-------------------------------------------------------------------------------------------------

NX_REFLECTION_INSTRUMENT(CreateSystemOfferRequest, (toAccount)(organizationId)(systemId)(comment))

NX_REFLECTION_INSTRUMENT_ENUM(OfferStatus, offered, accepted, rejected)

NX_REFLECTION_INSTRUMENT(SystemOffer, (fromAccount)(toAccount)(organizationId)(systemId)(systemName)(comment)(status))

NX_REFLECTION_INSTRUMENT(SystemOfferPatch, (comment)(status))

NX_REFLECTION_INSTRUMENT(Attribute, (name)(value))

NX_REFLECTION_INSTRUMENT(SystemUsersBatchItem, (users)(systems)(roleIds)(attributes))

NX_REFLECTION_INSTRUMENT(CreateBatchRequest, (items))

NX_REFLECTION_INSTRUMENT(CreateBatchResponse, (batchId))

NX_REFLECTION_INSTRUMENT(BatchState, (status) (operations))

NX_REFLECTION_INSTRUMENT_ENUM(BatchStatus, inProgress, success, failure)

NX_REFLECTION_INSTRUMENT(BatchErrorInfo, (uncommitted))

NX_REFLECTION_INSTRUMENT(BatchItemErrorInfo, (description)(item))

NX_REFLECTION_INSTRUMENT(SystemCredentials, (id)(authKey))

} // namespace nx::cloud::db::api
