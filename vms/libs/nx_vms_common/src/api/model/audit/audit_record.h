// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef _QN_AUDIT_RECORD_H__
#define _QN_AUDIT_RECORD_H__

#include <nx/utils/uuid.h>
#include "common/common_globals.h"
#include <nx/fusion/model_functions_fwd.h>
#include "auth_session.h"
#include "nx/utils/latin1_array.h"
#include <nx/utils/qnbytearrayref.h>

struct NX_VMS_COMMON_API QnAuditRecord
{
    bool isLoginType() const { return eventType == Qn::AR_Login || eventType == Qn::AR_UnauthorizedLogin; }
    bool isPlaybackType() const { return eventType == Qn::AR_ViewArchive || eventType == Qn::AR_ViewLive || eventType == Qn::AR_ExportVideo; }
    bool isCameraType() const { return eventType == Qn::AR_CameraUpdate || eventType == Qn::AR_CameraInsert || eventType == Qn::AR_CameraRemove; }

    QnByteArrayConstRef extractParam(const QnLatin1Array& name) const;
    void addParam(const QnLatin1Array& name, const QnLatin1Array& value);
    void removeParam(const QnLatin1Array& name);

    bool operator==(const QnAuditRecord& other) const = default;

    int createdTimeSec = 0; // audit record creation time
    int rangeStartSec = 0; // payload range start. Viewed archive range for playback or session time for login.
    int rangeEndSec = 0; // payload range start end. Viewed archive range for playback or session time for login.
    Qn::AuditRecordType eventType = Qn::AR_NotDefined;

    std::vector<QnUuid> resources;

    /**%apidoc
     * JSON object serialized using the Latin-1 encoding, even though it may contain other Unicode
     * characters.
     */
    QnLatin1Array params;

    QnAuthSession authSession;
};
#define QnAuditRecord_Fields \
    (createdTimeSec)(rangeStartSec)(rangeEndSec)(eventType)(resources)(params)(authSession)
QN_FUSION_DECLARE_FUNCTIONS(
    QnAuditRecord, (ubjson)(xml)(csv_record)(sql_record), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(QnAuditRecord, QnAuditRecord_Fields)

using QnAuditRecordList = QVector<QnAuditRecord>;
using QnAuditRecordRefList = QVector<QnAuditRecord*>;

Q_DECLARE_METATYPE(QnAuditRecord)
Q_DECLARE_METATYPE(QnAuditRecord*)
Q_DECLARE_METATYPE(QnAuditRecordList)

#endif // _QN_AUDIT_RECORD_H__
