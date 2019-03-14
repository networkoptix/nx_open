/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_PING_RESPONSE_H
#define NX_PING_RESPONSE_H

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/timestamp.h>

//!mediaserver's response to \a ping request
class QnPingReply
{
public:
    QnPingReply(): sysIdTime(0) {}

    QnUuid moduleGuid;
    QnUuid localSystemId;
    qint64 sysIdTime;
    nx::vms::api::Timestamp tranLogTime;
};

#define QnPingReply_Fields (moduleGuid)(localSystemId)(sysIdTime)(tranLogTime)

QN_FUSION_DECLARE_FUNCTIONS(QnPingReply, (json))

#endif  //NX_PING_RESPONSE_H
