#ifndef __QN_AUTH_SESSION_H__
#define __QN_AUTH_SESSION_H__

#include <utils/common/uuid.h>
#include <utils/common/model_functions_fwd.h>

struct QnAuthSession
{
    QnUuid sessionId;
    QString userName;
    QString userHost;
};

#define QnAuthSession_Fields (sessionId)(userName)(userHost)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnAuthSession), (eq))


#endif // __QN_AUTH_SESSION_H__
