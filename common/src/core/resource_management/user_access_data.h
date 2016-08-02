#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/literal.h>

namespace Qn {

struct UserAccessData
{
    enum class Access
    {
        Default,
        System,
        VideoWall,
        ClientConnection
    };

    QnUuid userId;
    Access access;

    UserAccessData();
    explicit UserAccessData(const QnUuid& userId, Access access = Access::Default);

    UserAccessData(const UserAccessData &other) = default;

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

/**
* Default kSystemAccess for server side is superuser.
* For client side kSystemAccess is not taken into account.
*/
const UserAccessData kSystemAccess(QnUuid(lit("{BC292159-2BE9-4E84-A242-BC6122B315E4}")), UserAccessData::Access::System);
const UserAccessData kVideowallUserAccess(QnUuid(lit("{1044D2A5-639D-4C49-963E-C03898D0C113}")), UserAccessData::Access::VideoWall);

}
