#ifndef __USER_ACCESS_DATA_H__
#define __USER_ACCESS_DATA_H__

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>

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

QnUserResourcePtr getUserResourceByAccessData(const UserAccessData &userAccessData);

/**
* Default kDefaultUserAccess for server side is superuser.
* For client side kDefaultUserAccess is not taken into account.
*/
const UserAccessData kDefaultUserAccess;
}

#endif // __USER_ACCESS_DATA_H__
