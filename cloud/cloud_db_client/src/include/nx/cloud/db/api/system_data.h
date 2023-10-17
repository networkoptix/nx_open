// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <map>

namespace nx::cloud::db::api {

/**
 * Information required to register system in cloud.
 */
struct SystemRegistrationData
{
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
    std::optional<std::string> systemId = std::nullopt;

    /**%apidoc Non-unique system name. */
    std::optional<std::string> name = std::nullopt;

    /**%apidoc Vms-specific data. Transparently stored and returned. */
    std::optional<std::string> opaque = std::nullopt;

    /**%apidoc If set to true, then cloud users wil be asked to enter 2FA code when logging
     * into this system.
     */
    std::optional<bool> system2faEnabled = std::nullopt;

    /**%apidoc
     * %deprecated Replaced by `mfaCode` attribute.
     * One-time password from the authenticator app.
     */
    std::optional<std::string> totp = std::nullopt;;

    /**%apidoc One-time password from the authenicator app.
     * Required and MUST be valid if changing system2faEnabled setting.
     */
    std::optional<std::string> mfaCode = std::nullopt;
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
    std::string ownerAccountEmail;
    SystemStatus status = SystemStatus::invalid;

    /**%apidoc DEPRECATED. true, if cloud connection is activated for this system. */
    bool cloudConnectionSubscriptionStatus = true;

    /**%apidoc MUST be used as upper 64 bits of 128-bit transaction timestamp. */
    std::uint64_t systemSequence = 0;

    /**%apidoc Vms-specific data. Same as SystemRegistrationData::opaque. */
    std::string opaque;

    /**%apidoc The VMS version reported by the last connected VMS server. */
    std::string version;

    std::chrono::system_clock::time_point registrationTime;

    /**%apidoc If true, then all cloud users are asked to use 2FA to log in to this system. */
    bool system2faEnabled = false;

    bool operator==(const SystemData& right) const
    {
        return
            id == right.id &&
            name == right.name &&
            customization == right.customization &&
            ownerAccountEmail == right.ownerAccountEmail &&
            status == right.status &&
            cloudConnectionSubscriptionStatus == right.cloudConnectionSubscriptionStatus &&
            systemSequence == right.systemSequence &&
            opaque == right.opaque &&
            system2faEnabled == right.system2faEnabled;
    }
};

struct SystemDataList
{
    /**%apidoc Systems attributes. */
    std::vector<SystemData> systems;
};


////////////////////////////////////////////////////////////
//// system sharing data
////////////////////////////////////////////////////////////

enum class SystemAccessRole
{
    none = 0,
    disabled,
    custom,
    liveViewer,
    viewer,
    advancedViewer,
    localAdmin,
    cloudAdmin,
    maintenance,
    owner,

    /**%apidoc This special value is used when the system is being requested using the system
     * credentials (likely, by a mediaserver).
     */
    system,
};

struct SystemSharing
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

    //TODO #akolesnikov this field is redundant here. Move it to libcloud_db internal data structures
    std::string vmsUserId;

    bool operator<(const SystemSharing& rhs) const
    {
        if (accountEmail != rhs.accountEmail)
            return accountEmail < rhs.accountEmail;
        return systemId < rhs.systemId;
    }

    bool operator==(const SystemSharing& rhs) const
    {
        return accountEmail == rhs.accountEmail
            && systemId == rhs.systemId
            && accessRole == rhs.accessRole
            && userRoleId == rhs.userRoleId
            && customPermissions == rhs.customPermissions
            && isEnabled == rhs.isEnabled;
    }
};

struct SystemSharingList
{
    /**%apidoc List of accounts the system has been shared with. */
    std::vector<SystemSharing> sharing;
};

/**
 * Expands SystemSharing to contain more data.
 */
struct SystemSharingEx: SystemSharing
{
    /**%apidoc Globally unique account id. */
    std::string accountId;

    std::string accountFullName;

    /**%apidoc Shows how often user accesses given system in comparison to other user's systems. */
    float usageFrequency = 0.0;

    /**%apidoc UTC time of the last login of user to the system. */
    std::chrono::system_clock::time_point lastLoginTime;

    bool operator==(const SystemSharingEx& rhs) const
    {
        return static_cast<const SystemSharing&>(*this) == static_cast<const SystemSharing&>(rhs)
            && accountId == rhs.accountId
            && accountFullName == rhs.accountFullName;
    }

    bool operator<(const SystemSharingEx& rhs) const
    {
        return std::tie(accountId, systemId) < std::tie(rhs.accountId, rhs.systemId);
    }
};

struct SystemSharingExList
{
    /**%apidoc List of accounts the system has been shared with. */
    std::vector<SystemSharingEx> sharing;
};

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

struct SystemDataEx: SystemData
{
    std::string ownerFullName;

    /**%apidoc Access role of the entity (usually, an account) that requested the system information. */
    SystemAccessRole accessRole = SystemAccessRole::none;

    /**%apidoc Permissions, account can share current system with. */
    std::vector<SystemAccessRoleData> sharingPermissions;

    SystemHealth stateOfHealth = SystemHealth::offline;

    /**%apidoc This number shows how often user, performing request,
     * uses this system in comparision to other systems.
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

struct SystemDataExList
{
    std::vector<SystemDataEx> systems;
};

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
    /**%apidoc The system to merge to another system. This is the system that disappears
     * during the merge. */
    std::string systemId;

    /**%apidoc OAUTH access token valid for authenticating requests to the system that stays
     * after the merge.
     * Required when merging 5.0+ systems.
     */
    std::optional<std::string> masterSystemAccessToken;

     /**%apidoc OAUTH access token valid for authenticating requests to the system that disappers
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

//-------------------------------------------------------------------------------------------------

struct SystemCredentials
{
    std::string id;
    std::string authKey;
};

} // namespace nx::cloud::db::api
