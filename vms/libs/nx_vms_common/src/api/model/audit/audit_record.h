// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/qnbytearrayref.h>
#include <nx/utils/uuid.h>

#include "auth_session.h"
#include "audit_details.h"

struct QnAuditRecord;

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

    QnAuthSession authSession;

    explicit operator QnAuditRecord() const;
};
#define QnLegacyAuditRecord_Fields \
    (createdTimeSec)(rangeStartSec)(rangeEndSec)(eventType)(resources)(params)(authSession)
QN_FUSION_DECLARE_FUNCTIONS(
    QnLegacyAuditRecord, (ubjson)(json)(sql_record), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(QnLegacyAuditRecord, QnLegacyAuditRecord_Fields)

using QnLegacyAuditRecordList = QVector<QnLegacyAuditRecord>;
using QnLegacyAuditRecordRefList = QVector<QnLegacyAuditRecord*>;

struct NX_VMS_COMMON_API QnAuditRecord
{
    bool operator==(const QnAuditRecord& other) const = default;

    /**%apidoc Record type. */
    nx::vms::api::AuditRecordType eventType = nx::vms::api::AuditRecordType::notDefined;

    /**%apidoc Record creation time in seconds. */
    std::chrono::seconds createdTimeS{0};

    /**%apidoc Information about the user whose actions created the record. */
    QnAuthSession authSession;

    /**%apidoc Detailed information about what's happened. */
    AllAuditDetails::type details = nullptr;

    template<typename T>
    T* get()
    {
        return details::get_if<T>(&details);
    }

    template<typename T>
    const T* get() const
    {
        return details::get_if<T>(&details);
    }

    explicit operator QnLegacyAuditRecord() const;

    template <nx::vms::api::AuditRecordType type,
        typename Details = typename details::details_type<type, AllAuditDetails::mapping>::type>
    static QnAuditRecord prepareRecord(const QnAuthSession& authInfo, Details&& details = {})
    {
        static_assert(
            std::is_same_v<typename details::details_type<type, AllAuditDetails::mapping>::type,
            std::decay_t<Details>>);
        QnAuditRecord result = prepareRecord(type, authInfo);
        result.details.emplace<std::decay_t<Details>>(std::forward<Details>(details));
        return result;
    }

    static QnAuditRecord prepareRecord(nx::vms::api::AuditRecordType type, const QnAuthSession& authInfo);
};
#define QnAuditRecord_Fields (eventType)(createdTimeS)(authSession)(details)
QN_FUSION_DECLARE_FUNCTIONS(QnAuditRecord, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(QnAuditRecord, QnAuditRecord_Fields)

struct NX_VMS_COMMON_API QnAuditRecordModel: QnAuditRecord
{
    QnAuditRecordModel() = default;
    QnAuditRecordModel(const QnAuditRecord& record);

    void setRangeStartSec(int value);
    void setRangeEndSec(int value);
    int getRangeStartSec() const;
    int getRangeEndSec() const;
};
#define QnAuditRecordModel_Fields QnAuditRecord_Fields
QN_FUSION_DECLARE_FUNCTIONS(QnAuditRecordModel, (sql_record), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(QnAuditRecordModel, QnAuditRecordModel_Fields)

using QnAuditRecordList = QVector<QnAuditRecord>;
using QnAuditRecordRefList = QVector<QnAuditRecord*>;
