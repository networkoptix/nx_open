#ifndef __QN_AUTH_SESSION_H__
#define __QN_AUTH_SESSION_H__

#include <utils/common/uuid.h>
#include <utils/common/model_functions_fwd.h>

struct QnAuthSession
{
    QnUuid id;
    QString userName;
    QString userHost;
    QString userAgent;
};

#define QnAuthSession_Fields (id)(userName)(userHost)(userAgent)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnAuthSession), (ubjson)(xml)(json)(csv_record)(eq)(sql_record))


#endif // __QN_AUTH_SESSION_H__
