#ifndef __QN_AUTH_SESSION_H__
#define __QN_AUTH_SESSION_H__

#include "utils/common/uuid.h"

struct QnAuthSession
{
    QnUuid sessionId;
    QString userName;
    QString userHost;
};


#endif // __QN_AUTH_SESSION_H__
