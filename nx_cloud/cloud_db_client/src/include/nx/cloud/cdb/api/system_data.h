#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

#include <boost/optional.hpp>

namespace nx {
namespace cdb {
namespace api {

/**
 * Information required to register system in cloud.
 */
class SystemRegistrationData
{
public:
    /** Not unique system name. */
    std::string name;
    std::string customization;
    /** Vms-specific data. Transparently stored and returned. */
    std::string opaque;
};

class SystemAttributesUpdate
{
public:
    std::string systemId;
    boost::optional<std::string> name;
    boost::optional<std::string> opaque;
};

enum class SystemStatus
{
    invalid = 0,
    /**
     * System has been bound but not a single request from
     * that system has been received by cloud.
     */
    notActivated,
    activated,
    deleted_,
};

class SystemData
{
public:
    /** Globally unique id of system assigned by cloud. */
    std::string id;
    /** Not unique system name. */
    std::string name;
    std::string customization;
    /** Key, system uses to authenticate requests to any cloud module. */
    std::string authKey;
    std::string ownerAccountEmail;
    SystemStatus status;
    /** true, if cloud connection is activated for this system. */
    bool cloudConnectionSubscriptionStatus;
    /** MUST be used as upper 64 bits of 128-bit transaction timestamp. */
    std::uint64_t systemSequence;
    /** Vms-specific data. Same as SystemRegistrationData::opaque. */
    std::string opaque;
    std::chrono::system_clock::time_point registrationTime;

    SystemData():
        status(SystemStatus::invalid),
        cloudConnectionSubscriptionStatus(true),
        systemSequence(0)
    {
    }

    bool operator==(const SystemData& right) const
    {
        return
            id == right.id &&
            name == right.name &&
            customization == right.customization &&
            authKey == right.authKey &&
            ownerAccountEmail == right.ownerAccountEmail &&
            status == right.status &&
            cloudConnectionSubscriptionStatus == right.cloudConnectionSubscriptionStatus &&
            systemSequence == right.systemSequence &&
            opaque == right.opaque;
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
    disabled = 1,
    custom = 2,
    liveViewer = 3,
    viewer = 4,
    advancedViewer = 5,
    localAdmin = 6,
    cloudAdmin = 7,
    maintenance = 8,
    owner = 9,
};

class SystemSharing
{
public:
    std::string accountEmail;
    std::string systemId;
    SystemAccessRole accessRole;
    std::string userRoleId;
    std::string customPermissions;
    bool isEnabled;
    //TODO #ak this field is redundant here. Move it to libcloud_db internal data structures
    std::string vmsUserId;

    SystemSharing():
        accessRole(SystemAccessRole::none),
        isEnabled(true)
    {
    }

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
    SystemSharingEx():
        usageFrequency(0.0)
    {
    }

    /** Globally unique account id. */
    std::string accountId;
    std::string accountFullName;
    /** Shows how often user accesses given system in comparison to other user's systems. */
    float usageFrequency;
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
    online
};

class SystemDataEx:
    public SystemData
{
public:
    std::string ownerFullName;
    SystemAccessRole accessRole;
    /** Permissions, account can share current system with. */
    std::vector<SystemAccessRoleData> sharingPermissions;
    SystemHealth stateOfHealth;
    /**
     * This number shows how often user, performing request,
     * uses this system in comparision to other systems.
     */
    float usageFrequency;
    /**
     * Time of last reported login of authenticated user to this system.
     * @note Fact of login is reported by SystemManager::recordUserSessionStart()
     */
    std::chrono::system_clock::time_point lastLoginTime;

    SystemDataEx():
        accessRole(SystemAccessRole::none),
        stateOfHealth(SystemHealth::offline),
        usageFrequency(0)
    {
    }

    SystemDataEx(SystemData systemData):
        SystemData(std::move(systemData)),
        accessRole(SystemAccessRole::none),
        stateOfHealth(SystemHealth::offline),
        usageFrequency(0)
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
    boost::optional<std::string> accountEmail;
    boost::optional<std::string> systemId;
};

enum class FilterField
{
    customization,
};

class Filter
{
public:
    std::map<FilterField, std::string> nameToValue;
};

} // namespace api
} // namespace cdb
} // namespace nx
