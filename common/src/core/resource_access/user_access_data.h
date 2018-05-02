#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/literal.h>

namespace Qn {

/** Helper class to grant additional permissions (on the server side only). */
struct UserAccessData
{
    enum class Access
    {
        Default,            /**< Default access rights. All permissions are checked as usual. */
        ReadAllResources,   /**< Read permission on all resources is granted additionally. */
        System              /**< Full permissions on all transactions. */
    };

    QnUuid userId;
    Access access;

    UserAccessData();
    explicit UserAccessData(const QnUuid& userId, Access access = Access::Default);

    UserAccessData(const UserAccessData&) = default;

    bool isNull() const { return userId.isNull(); }
};

inline bool operator == (const UserAccessData &lhs, const UserAccessData &rhs)
{
    return lhs.userId == rhs.userId && lhs.access == rhs.access;
}

inline bool operator != (const UserAccessData &lhs, const UserAccessData &rhs)
{
    return ! operator == (lhs, rhs);
}

const UserAccessData kSystemAccess(
    QnUuid(lit("{BC292159-2BE9-4E84-A242-BC6122B315E4}")),
    UserAccessData::Access::System);

const UserAccessData kVideowallUserAccess(
    QnUuid(lit("{1044D2A5-639D-4C49-963E-C03898D0C113}")),
    UserAccessData::Access::ReadAllResources);

}
