/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_PING_RESPONSE_H
#define NX_PING_RESPONSE_H

#include <QtCore/QUuid>

#include <utils/common/model_functions_fwd.h>


//!mediaserver's response to \a ping request
class QnPingReply
{
public:
    QUuid moduleGuid;
};

#define QnPingReply_Fields (moduleGuid)

QN_FUSION_DECLARE_FUNCTIONS(QnPingReply, (json))

#endif  //NX_PING_RESPONSE_H
