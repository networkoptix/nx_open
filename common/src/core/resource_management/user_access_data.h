#ifndef __USER_ACCESS_DATA_H__
#define __USER_ACCESS_DATA_H__

#include <nx/utils/uuid.h>

namespace Qn
{
struct UserAccessData
{
    QnUuid userId;

    UserAccessData() {}
    UserAccessData(const QnUuid &userId) : userId(userId) {}

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

const UserAccessData kSuperUserAccess;
}

#endif // __USER_ACCESS_DATA_H__
