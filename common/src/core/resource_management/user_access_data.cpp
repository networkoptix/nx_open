#include "user_access_data.h"

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

} //namespace Qn
