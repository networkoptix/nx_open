#ifndef _QN_AUDIT_RECORD_H__
#define _QN_AUDIT_RECORD_H__

#include <nx/utils/uuid.h>
#include "common/common_globals.h"
#include <nx/fusion/model_functions_fwd.h>
#include "auth_session.h"
#include "nx/utils/latin1_array.h"
#include <nx/utils/qnbytearrayref.h>

struct QnAuditRecord
{
    QnAuditRecord(): createdTimeSec(0), rangeStartSec(0), rangeEndSec(0), eventType(Qn::AR_NotDefined) {}

    bool isLoginType() const { return eventType == Qn::AR_Login || eventType == Qn::AR_UnauthorizedLogin; }
    bool isPlaybackType() const { return eventType == Qn::AR_ViewArchive || eventType == Qn::AR_ViewLive || eventType == Qn::AR_ExportVideo; }
    bool isCameraType() const { return eventType == Qn::AR_CameraUpdate || eventType == Qn::AR_CameraInsert || eventType == Qn::AR_CameraRemove; }

    QnByteArrayConstRef extractParam(const QnLatin1Array& name) const;
    void addParam(const QnLatin1Array& name, const QnLatin1Array& value);
    void removeParam(const QnLatin1Array& name);

    int createdTimeSec; // audit record creation time
    int rangeStartSec; // payload range start. Viewed archive range for playback or session time for login.
    int rangeEndSec; // payload range start end. Viewed archive range for playback or session time for login.
    Qn::AuditRecordType eventType;
    
    std::vector<QnUuid> resources;
    QnLatin1Array params; // I didn't use map here for optimization reason

    QnAuthSession authSession;
};
typedef QVector<QnAuditRecord> QnAuditRecordList;
typedef QVector<QnAuditRecord*> QnAuditRecordRefList;


#define QnAuditRecord_Fields (createdTimeSec)(rangeStartSec)(rangeEndSec)(eventType)(resources)(params)(authSession)
Q_DECLARE_METATYPE(QnAuditRecord)
Q_DECLARE_METATYPE(QnAuditRecord*)
Q_DECLARE_METATYPE(QnAuditRecordList)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnAuditRecord), (ubjson)(xml)(json)(csv_record)(eq)(sql_record))


#endif // _QN_AUDIT_RECORD_H__
