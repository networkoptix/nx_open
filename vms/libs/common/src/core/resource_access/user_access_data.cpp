#include "user_access_data.h"

#include <nx/utils/log/log.h>

namespace Qn {

UserAccessData::UserAccessData():
    userId(),
    access(Access::Default)
{
}

UserAccessData::UserAccessData(const QnUuid& userId, Access access):
    userId(userId),
    access(access)
{
}

QString UserAccessData::toString() const
{
    return lm("%1(%2)").args(access, userId);
}

QString toString(UserAccessData::Access access)
{
    switch (access)
    {
        case UserAccessData::Access::Default: return lit("Default");
        case UserAccessData::Access::ReadAllResources: return lit("ReadAllResources");
        case UserAccessData::Access::System: return lit("System");
    }

    const auto error = lm("Unknown=%1").arg(static_cast<int>(access));
    NX_ASSERT(false, error);
    return error;
}

} //namespace Qn
