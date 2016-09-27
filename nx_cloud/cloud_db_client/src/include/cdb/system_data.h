#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <boost/optional.hpp>

namespace nx {
namespace cdb {
namespace api {

/** Unused currently. */
class SubscriptionData
{
public:
    std::string productID;
    std::string systemID;
};

/**
 * Information required to register system in cloud.
 */
class SystemRegistrationData
{
public:
    /** Not unique system name. */
    std::string name;
    std::string customization;
};

enum class SystemStatus
{
    // TODO: #ak remove "ss" prefix.
    ssInvalid = 0,
    /**
     * System has been bound but not a single request from 
     * that system has been received by cloud.
     */
    ssNotActivated,
    ssActivated,
    ssDeleted
};

class SystemData
{
public:
    /** Globally unique ID of system assigned by cloud. */
    std::string id;
    /** Not unique system name. */
    std::string name;
    std::string customization;
    /** Key, system uses to authenticate requests to any cloud module. */
    std::string authKey;
    std::string ownerAccountEmail;
    SystemStatus status;
    /** \a true, if cloud connection is activated for this system. */
    bool cloudConnectionSubscriptionStatus;
    /** MUST be used as upper 64 bits of 128-bit transaction timestamp. */
    std::uint64_t systemSequence;

    SystemData()
    :
        status(SystemStatus::ssInvalid),
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
            systemSequence == right.systemSequence;
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
    std::string systemID;
    SystemAccessRole accessRole;
    std::string groupID;
    std::string customPermissions;
    bool isEnabled;
    //TODO #ak this field is redundant here. Move it to libcloud_db internal data structures
    std::string vmsUserId;

    SystemSharing()
    :
        accessRole(SystemAccessRole::none),
        isEnabled(true)
    {
    }

    bool operator<(const SystemSharing& rhs) const
    {
        if (accountEmail != rhs.accountEmail)
            return accountEmail < rhs.accountEmail;
        return systemID < rhs.systemID;
    }
    bool operator==(const SystemSharing& rhs) const
    {
        return accountEmail == rhs.accountEmail
            && systemID == rhs.systemID
            && accessRole == rhs.accessRole
            && groupID == rhs.groupID
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
 * Expands \a SystemSharing to contain account's full name.
 */
class SystemSharingEx
:
    public SystemSharing
{
public:
    SystemSharingEx() {}

    /** Globally unique account id. */
    std::string accountID;
    std::string accountFullName;

    bool operator==(const SystemSharingEx& rhs) const
    {
        return static_cast<const SystemSharing&>(*this) == static_cast<const SystemSharing&>(rhs)
            && accountID == rhs.accountID
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

    SystemAccessRoleData()
    :
        accessRole(SystemAccessRole::none)
    {
    }

    SystemAccessRoleData(SystemAccessRole _accessRole)
    :
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
    offline,
    online
};

class SystemDataEx
:
    public SystemData
{
public:
    std::string ownerFullName;
    SystemAccessRole accessRole;
    /** Permissions, account can share current system with. */
    std::vector<SystemAccessRoleData> sharingPermissions;
    SystemHealth stateOfHealth;
    /** This number shows how often user, performing request, 
     * uses this system in comparision to other systems.
     */
    float accessWeight;

    SystemDataEx()
    :
        accessRole(SystemAccessRole::none),
        stateOfHealth(SystemHealth::offline),
        accessWeight(0)
    {
    }

    SystemDataEx(SystemData systemData)
    :
        SystemData(std::move(systemData)),
        accessRole(SystemAccessRole::none),
        stateOfHealth(SystemHealth::offline),
        accessWeight(0)
    {
    }
};

class SystemDataExList
{
public:
    std::vector<SystemDataEx> systems;
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

} // namespace api
} // namespace cdb
} // namespace nx
