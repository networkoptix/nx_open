#ifndef __USER_ACCESS_DATA_H__
#define __USER_ACCESS_DATA_H__

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/literal.h>

namespace Qn
{
struct UserAccessData
{
    QnUuid userId;

    UserAccessData() {}
    explicit UserAccessData(const QnUuid &userId) : userId(userId) {}

    UserAccessData(const UserAccessData &other) = default;

    bool isNull() const { return userId.isNull(); }
};

inline bool operator == (const UserAccessData &lhs, const UserAccessData &rhs)
{
    return lhs.userId == rhs.userId;
}

inline bool operator != (const UserAccessData &lhs, const UserAccessData &rhs)
{
    return ! operator == (lhs, rhs);
}

/**
* Default kDefaultUserAccess for server side is superuser.
* For client side kDefaultUserAccess is not taken into account.
*/
const UserAccessData kDefaultUserAccess(QnUuid(lit("{BC292159-2BE9-4E84-A242-BC6122B315E4}")));
const UserAccessData kVideowallUserAccess(QnUuid(lit("{1044D2A5-639D-4C49-963E-C03898D0C113}")));
}

#endif // __USER_ACCESS_DATA_H__
