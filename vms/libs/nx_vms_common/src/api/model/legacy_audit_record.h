// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/rest/auth_session.h>
#include <nx/utils/qnbytearrayref.h>
#include <nx/vms/api/data/audit.h>

struct NX_VMS_COMMON_API QnLegacyAuditRecord
{
    bool isLoginType() const { return eventType == Qn::AR_Login || eventType == Qn::AR_UnauthorizedLogin; }
    bool isPlaybackType() const { return eventType == Qn::AR_ViewArchive || eventType == Qn::AR_ViewLive || eventType == Qn::AR_ExportVideo; }
    bool isCameraType() const { return eventType == Qn::AR_CameraUpdate || eventType == Qn::AR_CameraInsert || eventType == Qn::AR_CameraRemove; }

    QnByteArrayConstRef extractParam(const QnLatin1Array& name) const;
    void addParam(const QnLatin1Array& name, const QnLatin1Array& value);
    void removeParam(const QnLatin1Array& name);

    bool operator==(const QnLegacyAuditRecord& other) const = default;

    int createdTimeSec = 0; // audit record creation time
    int rangeStartSec = 0; // payload range start. Viewed archive range for playback or session time for login.
    int rangeEndSec = 0; // payload range start end. Viewed archive range for playback or session time for login.
    Qn::LegacyAuditRecordType eventType = Qn::AR_NotDefined;

    std::vector<nx::Uuid> resources;

    /**%apidoc
     * JSON object serialized using the Latin-1 encoding, even though it may contain other Unicode
     * characters.
     */
    QnLatin1Array params;

    nx::network::rest::AuthSession authSession;

    QnLegacyAuditRecord(nx::network::rest::AuthSession authSession = nx::Uuid{}):
        authSession(std::move(authSession))
    {
    }

    QnLegacyAuditRecord(nx::vms::api::AuditRecord auditRecord);
    explicit operator nx::vms::api::AuditRecord() const;
};
#define QnLegacyAuditRecord_Fields \
    (createdTimeSec)(rangeStartSec)(rangeEndSec)(eventType)(resources)(params)(authSession)
QN_FUSION_DECLARE_FUNCTIONS(
    QnLegacyAuditRecord, (ubjson)(json)(sql_record), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(QnLegacyAuditRecord, QnLegacyAuditRecord_Fields)

using QnLegacyAuditRecordList = QVector<QnLegacyAuditRecord>;
using QnLegacyAuditRecordRefList = QVector<QnLegacyAuditRecord*>;
