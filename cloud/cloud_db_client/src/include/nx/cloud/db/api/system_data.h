// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/std/cpp20.h>

#include "permissions.h"

namespace nx::cloud::db::api {

/**
 * System attribute.
 */
struct Attribute
{
    /**%apidoc Unique attribute name. */
    std::string name;

    /**%apidoc Attribute value. */
    std::string value;

    bool operator==(const Attribute& rhs) const
    {
        return name == rhs.name && value == rhs.value;
    }
};

/**%apidoc Array of Attributes. */
using AttributesList = std::vector<Attribute>;

std::string getAttrValueOr(
    const AttributesList& attrs, const std::string& name, const std::string& defaultV);

/**
 * Information required to register system in cloud.
 */
struct SystemRegistrationData
{
    /**%apidoc If specified, then an attempt to assign this id will be made.
     * If the id was alredy taken, an error is raised.
     */
    std::optional<std::string> id = std::nullopt;

    /**%apidoc Non-unique system name. */
    std::string name;

    std::string customization;

    /**%apidoc Vms-specific data. Transparently stored and returned. */
    std::string opaque;
};

/**
 * An update to apply to a system.
 */
struct SystemAttributesUpdate
{
    std::optional<std::string> systemId;

    /**%apidoc Non-unique system name. */
    std::optional<std::string> name;

    /**%apidoc Vms-specific data. Transparently stored and returned. */
    std::optional<std::string> opaque;

    /**%apidoc If set to true, then cloud users will be asked to enter 2FA code when logging
     * into this system.
     */
    std::optional<bool> system2faEnabled;

    /**%apidoc
     * %deprecated Replaced by `mfaCode` attribute.
     * One-time password from the authenticator app.
     */
    std::optional<std::string> totp;

    /**%apidoc One-time password from the authenticator app.
     * Required and MUST be valid if changing system2faEnabled setting.
     */
    std::optional<std::string> mfaCode;
};

enum class SystemStatus
{
    invalid = 0,

    /**%apidoc System has been bound but not a single request from
     * that system has been received by cloud.
     */
    notActivated,

    activated,
    deleted_,
    beingMerged,
    deletedByMerge,
};

struct SystemData
{
    /**%apidoc Globally unique id of system assigned by cloud. */
    std::string id;

    /**%apidoc Non-unique system name. */
    std::string name;
    std::string customization;

    /**%apidoc Key, system uses to authenticate requests to any cloud module. */
    std::string authKey;

    // TODO: #akolesnikov. This field does not belong here. It is not a part of the API.
    std::string authKeyHash;
    SystemStatus status = SystemStatus::invalid;

    /**%apidoc MUST be used as upper 64 bits of 128-bit transaction timestamp. */
    std::uint64_t systemSequence = 0;

    /**%apidoc Vms-specific data. Same as SystemRegistrationData::opaque. */
    std::string opaque;

    /**%apidoc The VMS version reported by the last connected VMS server. */
    std::string version;

    std::chrono::system_clock::time_point registrationTime;

    /**%apidoc If true, then all cloud users are asked to use 2FA to log in to this system. */
    bool system2faEnabled = false;

    // System attributes. These attributes are present on the same level with other fields
    // of this structure in the JSON document. They are NOT represented as an "attributes" array
    // in the JSON document.
    AttributesList attributes;

    std::string organizationId;

    bool operator==(const SystemData& right) const
    {
        return
            id == right.id &&
            name == right.name &&
            customization == right.customization &&
            status == right.status &&
            systemSequence == right.systemSequence &&
            opaque == right.opaque &&
            system2faEnabled == right.system2faEnabled &&
            organizationId == right.organizationId;
    }

    std::string ownerAccountEmail() const
    {
        return getAttrValueOr(attributes, "ownerAccountEmail", std::string());
    }

    std::string ownerFullName() const
    {
        return getAttrValueOr(attributes, "ownerFullName", std::string());
    }
};

#define SystemData_Fields (id)(name)(customization)(authKey)(authKeyHash) \
    (status)(systemSequence) (opaque)(registrationTime)(system2faEnabled)(attributes)(organizationId)

NX_REFLECTION_INSTRUMENT(SystemData, SystemData_Fields)

// Providing custom JSON serialization functions so that SystemData::attributes are added on the
// same level with other fields in the resulting JSON document.

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const SystemData& value);

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx,
    SystemData* value);

struct SystemDataList
{
    /**%apidoc Systems attributes. */
    std::vector<SystemData> systems;
};

NX_REFLECTION_INSTRUMENT(SystemDataList, (systems))

////////////////////////////////////////////////////////////
//// system sharing data
////////////////////////////////////////////////////////////

enum class SystemAccessRole
{
    none = 0,
    custom = 2,
    liveViewer = 3,
    viewer = 4,
    advancedViewer = 5,
    localAdmin = 6,
    cloudAdmin = 7,
    owner = 9,

    /**%apidoc This special value is used when the system is being requested using the system
     * credentials (likely, by a VMS server).
     */
    system = 10,
};

// Deprecated share system request. Preserved for the backward compatibility.
struct ShareSystemRequestV1
{
    /**%apidoc The account to share the system with. */
    std::string accountEmail;

    /**%apidoc The system to share. */
    std::string systemId;

    /**%apidoc System access role to give to the account. */
    SystemAccessRole accessRole = SystemAccessRole::none;

    /**%apidoc VMS-specific user role ID. For the cloud this is an opaque string that is sent
     * to the VMS server when adding user.
     */
    std::string userRoleId;

    /**%apidoc VMS-specific permissions. For the cloud this is an opaque string that is sent
     * to the VMS server when adding user.
     */
    std::string customPermissions;

    bool isEnabled = true;

    // User id to set in the VMS DB.
    std::string vmsUserId;

    bool operator<(const ShareSystemRequestV1& rhs) const
    {
        if (accountEmail != rhs.accountEmail)
            return accountEmail < rhs.accountEmail;
        return systemId < rhs.systemId;
    }

    bool operator==(const ShareSystemRequestV1& rhs) const
    {
        return accountEmail == rhs.accountEmail
            && systemId == rhs.systemId
            && accessRole == rhs.accessRole
            && userRoleId == rhs.userRoleId
            && customPermissions == rhs.customPermissions
            && isEnabled == rhs.isEnabled;
    }
};

/**
 * Describes an account to system relation.
 */
struct SystemSharingV1: ShareSystemRequestV1
{
    /**%apidoc Globally unique account id. */
    std::string accountId;

    std::string accountFullName;

    /**%apidoc Shows how often user accesses given system in comparison to other user's systems. */
    float usageFrequency = 0.0;

    /**%apidoc UTC time of the last login of user to the system. */
    std::chrono::system_clock::time_point lastLoginTime;

    bool operator==(const SystemSharingV1& rhs) const
    {
        return static_cast<const ShareSystemRequestV1&>(*this) == static_cast<const ShareSystemRequestV1&>(rhs)
            && accountId == rhs.accountId
            && accountFullName == rhs.accountFullName;
    }

    bool operator<(const SystemSharingV1& rhs) const
    {
        return std::tie(accountId, systemId) < std::tie(rhs.accountId, rhs.systemId);
    }
};

struct SystemSharingV1List
{
    /**%apidoc List of accounts the system has been shared with. */
    std::vector<SystemSharingV1> sharing;
};

/**
 * Request to share a system with a new user or to modify the access rights of an existing user.
 * Provides improved support for permissions and roles.
 * This is the current version of the request. Corresponds to the version 2 of the API.
 */
struct ShareSystemRequest
{
    /**%apidoc The account to share the system with. */
    std::string accountEmail;

    /**%apidoc List of roles to assign to the user.
     * This can be both cloud and VMS-specific roles. Cloud uses those roles it recognizes
     * while authorizing user requests.
     */
    std::vector<std::string> roleIds;

    /**%apidoc List of specific permissions to grant to the user. This extends the list of roles.
     * This can be both cloud and VMS-specific permissions. Cloud uses those permissions it
     * recognizes while authorizing user requests.
     */
    std::vector<std::string> permissions;

    /**%apidoc Allows to disable/re-enable user. Disabled user does not have access to the system.
     * But, still present in the system.
     */
    bool isEnabled = true;

    // User id to set in the VMS DB.
    std::string vmsUserId;

    // NOTE: Cannot use operator<=> due to lack of compiler support on some platforms.
    bool operator==(const ShareSystemRequest& rhs) const
    {
        return std::tie(accountEmail, roleIds, permissions, isEnabled, vmsUserId)
            == std::tie(rhs.accountEmail, rhs.roleIds, rhs.permissions, rhs.isEnabled, rhs.vmsUserId);
    }

    bool operator<(const ShareSystemRequest& rhs) const
    {
        return std::tie(accountEmail, roleIds, permissions, isEnabled, vmsUserId)
            < std::tie(rhs.accountEmail, rhs.roleIds, rhs.permissions, rhs.isEnabled, rhs.vmsUserId);
    }
};

NX_REFLECTION_INSTRUMENT(ShareSystemRequest, (accountEmail)(roleIds)(permissions)(isEnabled)(vmsUserId))

/**
 * Describes an account to system relation.
 */
struct SystemSharing: ShareSystemRequest
{
    using base_type = ShareSystemRequest;

    /**%apidoc Id of the system. */
    std::string systemId;

    /**%apidoc Globally unique account id. */
    std::string accountId;

    /**%apidoc Account full name. */
    std::string accountFullName;

    /**%apidoc Shows how often user accesses given system in comparison to other user's systems. */
    float usageFrequency = 0.0;

    /**%apidoc UTC time of the last login of user to the system. */
    std::chrono::system_clock::time_point lastLoginTime;

    bool operator==(const SystemSharing& rhs) const
    {
        return std::tie(static_cast<const base_type&>(*this), systemId, accountId, accountFullName)
            == std::tie(static_cast<const base_type&>(rhs), rhs.systemId, rhs.accountId,
                rhs.accountFullName);
    }

    bool operator<(const SystemSharing& rhs) const
    {
        return std::tie(static_cast<const base_type&>(*this), systemId, accountId, accountFullName)
            < std::tie(static_cast<const base_type&>(rhs), rhs.systemId, rhs.accountId,
                rhs.accountFullName);
    }
};

#define SystemSharing_Fields (systemId)(accountId)(accountFullName)(usageFrequency)(lastLoginTime) \
    (accountEmail)(roleIds)(permissions)(isEnabled)(vmsUserId)

NX_REFLECTION_INSTRUMENT(SystemSharing, (systemId)(accountId)(accountFullName)(usageFrequency) \
    (lastLoginTime))

using SystemSharingList = std::vector<SystemSharing>;

struct ShareSystemQuery
{
    /**%apidoc Specifies whether the "system shared with you" notification is sent or not. */
    std::optional<bool> sendNotification;
};

struct SystemAccessRoleData
{
    SystemAccessRole accessRole = SystemAccessRole::none;

    SystemAccessRoleData() = default;
    SystemAccessRoleData(SystemAccessRole accessRole): accessRole(accessRole) {}
};

struct SystemAccessRoleList
{
    std::vector<SystemAccessRoleData> accessRoles;
};

enum class SystemHealth
{
    offline = 0,

    /**%apidoc The system is online but it has the data synchronization protocol version greater
     * than the cloud.
     */
     online,

    /**%apidoc The system is online but it has the data synchronization protocol version greater
     * than the cloud. In the situation the VMS and the Cloud cannot interact properly.
     */
    incompatible,
};

enum class MergeRole
{
    none,
    /**%apidoc System is the resulting system. */
    master,
    /**%apidoc System is consumed in the process of the merge. */
    slave,
};

struct SystemMergeInfo
{
    /**%apidoc Role of the current system in the ongoing merge. The current system is the system
     * that was requested. Not the <i>anotherSystemId</i>!
     */
    MergeRole role = MergeRole::none;

    /**%apidoc UTC time of the merge start. */
    std::chrono::system_clock::time_point startTime;

    /**%apidoc Id of the system the current one is being merged with. */
    std::string anotherSystemId;
};

/**%apidoc Extended system information.
 * Adds information that is defined by the request context. E.g., user access level,
 * system health, etc...
 * Also, system attributes managed via a separate "system attributes" API are also present here.
 */
struct SystemDataEx: SystemData
{
    using base_type = SystemData;

    /**%apidoc Access role of the entity (usually, an account) that requested the system information. */
    SystemAccessRole accessRole = SystemAccessRole::none;

    SystemHealth stateOfHealth = SystemHealth::offline;

    /**%apidoc
     * This number shows how often the User performing the request uses this System in comparison
     * to other Systems.
     */
    float usageFrequency = 0;

    /**%apidoc Time of last reported login of authenticated user to this system.
     * Note: Fact of login is reported by SystemManager::recordUserSessionStart()
     */
    std::chrono::system_clock::time_point lastLoginTime;

    /**%apidoc Information about the ongoing merge of this system and another one.
     * Present only if there is a merge going on.
     */
    std::optional<SystemMergeInfo> mergeInfo;

    /**%apidoc dictionary{capability: capability version (0-disabled)}. */
    std::map<std::string, int> capabilities;

    SystemDataEx() = default;

    SystemDataEx(SystemData systemData):
        SystemData(std::move(systemData))
    {
    }
};

// NOTE: Have to move NX_REFLECTION_INSTRUMENT here because otherwise custom serialization functions
// will not been seen anywhere SystemDataEx type is used.
// TODO: #akolesnikov Move NX_REFLECTION_INSTRUMENT for other types here as well.
NX_REFLECTION_INSTRUMENT(SystemDataEx,
    (accessRole)(stateOfHealth)(usageFrequency)(lastLoginTime)(mergeInfo)(capabilities)(version))

// Providing custom JSON serialization functions so that SystemDataEx::attributes are added on the
// same level with other fields in the resulting JSON document.

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const SystemDataEx& value);

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx,
    SystemDataEx* value);

struct SystemDataExList
{
    std::vector<SystemDataEx> systems;
};

NX_REFLECTION_INSTRUMENT(SystemDataExList, (systems))

//-------------------------------------------------------------------------------------------------

struct SystemHealthHistoryItem
{
    /**%apidoc Timestamp of the event. */
    std::chrono::system_clock::time_point timestamp;

    /**%apidoc The status system switched to. */
    SystemHealth state = SystemHealth::offline;
};

struct SystemHealthHistory
{
    /**%apidoc System status change events. */
    std::vector<SystemHealthHistoryItem> events;
};

/**
 * Information about newly started user session.
 */
struct UserSessionDescriptor
{
    std::optional<std::string> accountEmail;
    std::optional<std::string> systemId;
};

struct MergeRequest
{
    /**%apidoc The system to merge to another system. This is the System that disappears
     * during the merge. */
    std::string systemId;

    /**%apidoc OAUTH access token valid for authenticating requests to the System that stays
     * after the merge.
     * Required when merging 5.0+ systems.
     */
    std::optional<std::string> masterSystemAccessToken;

     /**%apidoc OAUTH access token valid for authenticating requests to the System that disappears
      * during the merge.
      * Required when merging 5.0+ systems.
      */
    std::optional<std::string> slaveSystemAccessToken;
};

enum class FilterField
{
    customization,
    systemStatus,
};

struct Filter
{
    std::map<FilterField, std::string> nameToValue;
};

struct ValidateMSSignatureRequest
{
    /**%apidoc Opaque text. */
    std::string message;
    /**%apidoc SIGNATURE = base64(hmacSha256(cloudSystemAuthKey, message)) */
    std::string signature;
};

// TODO: #akolesnikov This structure appears to be redundant.
struct SystemIdList
{
    std::vector<std::string> systemIds;
};

struct GetSystemUsersRequest
{
    std::string systemId;
    bool localOnly = false;
};

//-------------------------------------------------------------------------------------------------

struct CreateSystemOfferRequest
{
    /**%apidoc The account the system is offered to. */
    std::string toAccount;

    /**%apidoc ID of offered system. */
    std::string systemId;

    /**%apidoc Any text. */
    std::string comment;
};

enum class OfferStatus
{
    offered = 1,
    accepted,
    rejected,
};

struct SystemOffer
{
    /**%apidoc The account that made the offer.
     * It is the current system owner if system has not been accepted yet.
     */
    std::string fromAccount;

    /**%apidoc The account the system has been offered to. */
    std::string toAccount;

    /**%apidoc Guess what. */
    std::string systemId;

    /**%apidoc Name of the system. */
    std::string systemName;

    /**%apidoc Any text. */
    std::string comment;

    /**%apidoc The current status of the offer. */
    OfferStatus status = OfferStatus::offered;
};

struct SystemOfferPatch
{
    /**%apidoc Any text. */
    std::optional<std::string> comment;

    /**%apidoc New status for the offer. */
    std::optional<OfferStatus> status;
};

/**
* One system users batch item.
*/
struct SystemUsersBatchItem
{
    /**%apidoc Users emails. */
    std::vector<std::string> users;

    /**%apidoc System ids. */
    std::vector<std::string> systems;

    /**%apidoc Roles to be assigned. Empty array is used for revoking access. */
    std::vector<std::string> roleIds;

    /**%apidoc Custom attributes to assign. */
    std::map<std::string, std::string> attributes;
};

/**
* Create async processing batch request.
*/
struct CreateBatchRequest
{
    /**%apidoc Batch items to process. */
    std::vector<SystemUsersBatchItem> items;
};

/**
 * Create batch response with request traking id assigned.
*/
struct CreateBatchResponse
{
    /**%apidoc Batch traking id*/
    std::string batchId;
};

/**
*  Current batch status.
*/
enum class BatchStatus
{
    /**apidoc Processing in progress*/
    inProgress = 1,
    /**apidoc Successfully processed all items*/
    success,
    /**apidoc Processed all items but contains some errors*/
    failure
};

/**
*  Batch state response.
*  Contains number of operations by status: pending, failed, complete
*  Batch has been processed completely if pending operations is zero
*  Batch hase been processed without errors if failed operations is zero
*/
struct BatchState
{
    /**%apidoc Current batch status. */
    BatchStatus status = BatchStatus::inProgress;
    /**%apidoc Number of operations by status: pending, failed, complete */
    std::map<std::string, int> operations;
};

/**
*  Failed to prcess batch item with error description.
*/
struct BatchItemErrorInfo
{
    /**apidoc Error description*/
    std::string description;

    /**apidoc Uncommitted item*/
    SystemUsersBatchItem item;
};

/**
*  Batch error response.
*/
struct BatchErrorInfo
{
    /**%apidoc Uncommited batch items */
    std::vector<BatchItemErrorInfo> uncommitted;
};

struct SystemCredentials
{
    std::string id;
    std::string authKey;
};

} // namespace nx::cloud::db::api
