#ifndef _QN_AUDIT_RECORD_H__
#define _QN_AUDIT_RECORD_H__

#include "utils/common/uuid.h"
#include "common/common_globals.h"
#include <utils/common/model_functions_fwd.h>

struct QnAuditRecord
{
    QnAuditRecord(): timestamp(0), endTimestamp(0), eventType(Qn::AR_NotDefined) {}

    int timestamp; // start timestamp in seconds
    int endTimestamp; // end timestamp in seconds
    Qn::AuditRecordType eventType;
    QnUuid sessionId;
    QString userName;
    QString userHost;
    QString description;
    std::vector<QnUuid> resources;
    QString params;
};
typedef std::vector<QnAuditRecord> QnAuditRecordList;


#define QnAuditRecord_Fields (timestamp)(endTimestamp)(eventType)(sessionId)(userName)(userHost)(description)(resources)(params)
Q_DECLARE_METATYPE(QnAuditRecord)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnAuditRecord), (ubjson)(xml)(json)(csv_record)(eq)(sql_record))


#endif // _QN_AUDIT_RECORD_H__
