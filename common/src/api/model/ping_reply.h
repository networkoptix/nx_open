/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_PING_RESPONSE_H
#define NX_PING_RESPONSE_H

#include <utils/common/uuid.h>

#include <utils/common/model_functions_fwd.h>


//!mediaserver's response to \a ping request
class QnPingReply
{
public:
    QnPingReply(): sysIdTime(0), tranLogTime(0) {}

    QnUuid moduleGuid;
    QString systemName;
    qint64 sysIdTime;
    qint64 tranLogTime;
};

#define QnPingReply_Fields (moduleGuid)(systemName)(sysIdTime)(tranLogTime)

QN_FUSION_DECLARE_FUNCTIONS(QnPingReply, (json))

#endif  //NX_PING_RESPONSE_H
