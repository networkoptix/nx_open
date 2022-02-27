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
class SystemRegistrationData
{
public:
    /**%apidoc Non-unique system name. */
    std::string name;

    std::string customization;

    /**%apidoc Vms-specific data. Transparently stored and returned. */
    std::string opaque;
};

class SystemAttributesUpdate
{
public:
    std::string systemId;
    std::optional<std::string> name;
    std::optional<std::string> opaque;
    std::optional<bool> system2faEnabled;
};

enum class SystemStatus
{
    invalid = 0,

    /**%apidoc
     * System has been bound but not a single request from
     * that system has been received by cloud.
     */
    notActivated,

    activated,
    deleted_,
    beingMerged,
    deletedByMerge,
};

class SystemData
{
public:
    /**%apidoc Globally unique id of system assigned by cloud. */
    std::string id;

    /**%apidoc Non-unique system name. */
    std::string name;
    std::string customization;

    /**%apidoc Key, system uses to authenticate requests to any cloud module. */
    std::string authKey;
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

class SystemDataList
{
public:
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
    system,
};

class SystemSharing
{
public:
    std::string accountEmail;
    std::string systemId;
    SystemAccessRole accessRole = SystemAccessRole::none;
    std::string userRoleId;
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

class SystemSharingList
{
public:
    std::vector<SystemSharing> sharing;
};

/**
 * Expands SystemSharing to contain more data.
 */
class SystemSharingEx:
    public SystemSharing
{
public:
    /**%apidoc Globally unique account id. */
    std::string accountId;

    std::string accountFullName;

    /**%apidoc Shows how often user accesses given system in comparison to other user's systems. */
    float usageFrequency = 0.0;

    std::chrono::system_clock::time_point lastLoginTime;

    bool operator==(const SystemSharingEx& rhs) const
    {
        return static_cast<const SystemSharing&>(*this) == static_cast<const SystemSharing&>(rhs)
            && accountId == rhs.accountId
            && accountFullName == rhs.accountFullName;
    }
};

class SystemSharingExList
{
public:
    std::vector<SystemSharingEx> sharing;
};

class SystemAccessRoleData
{
public:
    SystemAccessRole accessRole;

    SystemAccessRoleData():
        accessRole(SystemAccessRole::none)
    {
    }

    SystemAccessRoleData(SystemAccessRole _accessRole):
        accessRole(_accessRole)
    {
    }
};

class SystemAccessRoleList
{
public:
    std::vector<SystemAccessRoleData> accessRoles;
};

enum class SystemHealth
{
    offline = 0,
    online,
    incompatible
};

enum class MergeRole
{
    none,
    /**%apidoc System is the resulting system. */
    master,
    /**%apidoc System is consumed in the process of the merge. */
    slave,
};

class SystemMergeInfo
{
public:
    MergeRole role = MergeRole::none;
    std::chrono::system_clock::time_point startTime;
    std::string anotherSystemId;
};

class SystemDataEx:
    public SystemData
{
public:
    std::string ownerFullName;
    SystemAccessRole accessRole = SystemAccessRole::none;

    /**%apidoc Permissions, account can share current system with. */
    std::vector<SystemAccessRoleData> sharingPermissions;
    SystemHealth stateOfHealth = SystemHealth::offline;

    /**%apidoc
     * This number shows how often user, performing request,
     * uses this system in comparision to other systems.
     */
    float usageFrequency = 0;

    /**%apidoc
     * Time of last reported login of authenticated user to this system.
     * @note Fact of login is reported by SystemManager::recordUserSessionStart()
     */
    std::chrono::system_clock::time_point lastLoginTime;
    std::optional<SystemMergeInfo> mergeInfo;

    /**%apidoc map<capability, capability version (0-disabled)>. */
    std::map<std::string, int> capabilities;

    SystemDataEx() = default;

    SystemDataEx(SystemData systemData):
        SystemData(std::move(systemData))
    {
    }
};

class SystemDataExList
{
public:
    std::vector<SystemDataEx> systems;
};

class SystemHealthHistoryItem
{
public:
    std::chrono::system_clock::time_point timestamp;
    SystemHealth state;

    SystemHealthHistoryItem():
        state(SystemHealth::offline)
    {
    }
};

class SystemHealthHistory
{
public:
    std::vector<SystemHealthHistoryItem> events;
};

/**
 * Information about newly started user session.
 */
class UserSessionDescriptor
{
public:
    std::optional<std::string> accountEmail;
    std::optional<std::string> systemId;
};

enum class FilterField
{
    customization,
    systemStatus,
};

class Filter
{
public:
    std::map<FilterField, std::string> nameToValue;
};

struct ValidateMSSignatureRequest
{
    /**%apidoc Opaque text. */
    std::string message;
    /**%apidoc SIGNATURE = base64(hmacSha256(cloudSystemAuthKey, message)) */
    std::string signature;
};

} // namespace nx::cloud::db::api
