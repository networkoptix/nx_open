#ifndef _QN_AUDIT_RECORD_H__
#define _QN_AUDIT_RECORD_H__

#include "utils/common/uuid.h"
#include "common/common_globals.h"
#include <utils/common/model_functions_fwd.h>
#include "auth_session.h"

struct QnAuditRecord
{
    QnAuditRecord(): timestamp(0), endTimestamp(0), eventType(Qn::AR_NotDefined) {}

    void fillAuthInfo(const QnAuthSession& authInfo) 
    {
        sessionId = authInfo.sessionId;
        userName = authInfo.userName;
        userHost = authInfo.userHost;
    }

    int timestamp; // start timestamp in seconds
    int endTimestamp; // end timestamp in seconds
    Qn::AuditRecordType eventType;
    //QString description;
    std::vector<QnUuid> resources;
    QString extraParams;

    QnUuid sessionId;
    QString userName;
    QString userHost;
};
typedef QVector<QnAuditRecord> QnAuditRecordList;


#define QnAuditRecord_Fields (timestamp)(endTimestamp)(eventType)(resources)(extraParams)(sessionId)(userName)(userHost)
Q_DECLARE_METATYPE(QnAuditRecord)
Q_DECLARE_METATYPE(QnAuditRecordList)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnAuditRecord), (ubjson)(xml)(json)(csv_record)(eq)(sql_record))


#endif // _QN_AUDIT_RECORD_H__
