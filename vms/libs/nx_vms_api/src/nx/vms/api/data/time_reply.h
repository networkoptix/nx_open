#pragma once

#include <nx/vms/api/data_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <chrono>

namespace nx::vms::api {

using namespace std::chrono;

struct NX_VMS_API TimeReplyCore
{
    /** Utc time since epoch. */
    milliseconds osTime = 0ms;
    milliseconds vmsTime = 0ms;

    /** Time zone offset. */
    milliseconds timeZoneOffset = 0ms;
};

struct NX_VMS_API TimeReply : public TimeReplyCore
{
    QString timeZoneId;
    QString realm;
};
#define TimeReply_Fields (osTime)(vmsTime)(timeZoneOffset)(timeZoneId)(realm)

struct NX_VMS_API ServerTimeReply : public TimeReplyCore
{
    QnUuid serverId;
};
#define ServerTimeReply_Fields (osTime)(vmsTime)(timeZoneOffset)(serverId)

using ServerTimeReplyList = std::vector<ServerTimeReply>;

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::TimeReply)
Q_DECLARE_METATYPE(nx::vms::api::ServerTimeReply)
