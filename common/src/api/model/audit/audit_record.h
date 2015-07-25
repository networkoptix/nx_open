#ifndef _QN_AUDIT_RECORD_H__
#define _QN_AUDIT_RECORD_H__

#include "utils/common/uuid.h"
#include "common/common_globals.h"
#include <utils/common/model_functions_fwd.h>
#include "auth_session.h"
#include "utils/common/latin1_array.h"
#include "utils/network/http/qnbytearrayref.h"

struct QnAuditRecord
{
    QnAuditRecord(): createdTimeSec(0), rangeStartSec(0), rangeEndSec(0), eventType(Qn::AR_NotDefined) {}

    void fillAuthInfo(const QnAuthSession& authInfo) 
    {
        sessionId = authInfo.sessionId;
        userName = authInfo.userName;
        userHost = authInfo.userHost;
    }

    bool isLoginType() const { return eventType == Qn::AR_Login || eventType == Qn::AR_UnauthorizedLogin; }
    bool isPlaybackType() const { return eventType == Qn::AR_ViewArchive || eventType == Qn::AR_ViewLive; }

    QnByteArrayConstRef extractParam(const QnLatin1Array& name) const;
    void addParam(const QnLatin1Array& name, const QnLatin1Array& value);


    int createdTimeSec; // audit record creation time
    int rangeStartSec; // payload range start. Viewed archive range for playback or session time for login.
    int rangeEndSec; // payload range start end. Viewed archive range for playback or session time for login.
    Qn::AuditRecordType eventType;
    
    std::vector<QnUuid> resources;
    QnLatin1Array params; // I didn't use map here for optimization reason

    QnUuid sessionId;
    QString userName;
    QString userHost;
};
typedef QVector<QnAuditRecord> QnAuditRecordList;


#define QnAuditRecord_Fields (createdTimeSec)(rangeStartSec)(rangeEndSec)(eventType)(resources)(params)(sessionId)(userName)(userHost)
Q_DECLARE_METATYPE(QnAuditRecord)
Q_DECLARE_METATYPE(QnAuditRecordList)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnAuditRecord), (ubjson)(xml)(json)(csv_record)(eq)(sql_record))


#endif // _QN_AUDIT_RECORD_H__
