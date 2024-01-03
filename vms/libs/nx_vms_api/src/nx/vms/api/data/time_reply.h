// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>

namespace nx::vms::api {

using namespace std::chrono;

struct NX_VMS_API TimeReplyCore
{
    bool operator==(const TimeReplyCore& other) const = default;

    /**%apidoc
     * Local OS time on the Server, in milliseconds since epoch.
     * %deprecated
     */
    milliseconds osTime = 0ms;

    /**%apidoc
     * Synchronized time of the VMS Site, in milliseconds since epoch.
     * %deprecated
     */
    milliseconds vmsTime = 0ms;

    /**%apidoc
     * Time zone offset, in milliseconds.
     * %deprecated
     */
    milliseconds timeZoneOffset = 0ms;
};

struct NX_VMS_API TimeReply : public TimeReplyCore
{
    /**%apidoc Identification of the time zone in the text form. */
    QString timeZoneId;

    /**%apidoc Authentication realm. */
    QString realm;
};
#define TimeReply_Fields (osTime)(vmsTime)(timeZoneOffset)(timeZoneId)(realm)
NX_VMS_API_DECLARE_STRUCT(TimeReply)
NX_REFLECTION_INSTRUMENT(TimeReply, TimeReply_Fields);

struct NX_VMS_API ServerTimeReply : public TimeReplyCore
{
    QnUuid serverId;
};
#define ServerTimeReply_Fields (osTime)(vmsTime)(timeZoneOffset)(serverId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ServerTimeReply)

} // namespace nx::vms::api
