// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/rest/audit.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/qnbytearrayref.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/uuid.h>

#include "audit_details.h"

namespace QnCollection { struct list_tag; }

namespace QJsonDetail {

template<class Collection>
void serialize_collection(QnJsonContext*, const Collection&, QJsonValue*);

template<class Collection, class Element>
bool deserialize_collection_element(
    QnJsonContext*, const QJsonValue&, Collection*, const Element*, const QnCollection::list_tag&);

} // namespace QJsonDetail

struct QnAuditRecord;

struct NX_VMS_COMMON_API AuditLogFilter
{
    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`, or be `this` to
     * refer to the current Server, or be `*` to involve all Servers.
     * %example this
     */
    nx::Uuid serverId;

    /**%apidoc[opt]
     * Start time of a time interval, as a string containing time in milliseconds since epoch, or a
     * local time formatted like
     * <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code> -
     * the format is auto-detected.
     */
    QString from;

    /**%apidoc[opt]
     * End time of a time interval, as a string containing time in milliseconds since epoch, or a
     * local time formatted like
     * <code>"<i>YYYY</i>-<i>MM</i>-<i>DD</i>T<i>HH</i>:<i>mm</i>:<i>ss</i>.<i>zzz</i>"</code> -
     * the format is auto-detected.
     */
    QString to;

    /**%apidoc[opt] */
    nx::Uuid sessionId;
};
#define AuditLogFilter_Fields (serverId)(from)(to)(sessionId)
QN_FUSION_DECLARE_FUNCTIONS(AuditLogFilter, (json), NX_VMS_COMMON_API)

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
    /**%apidoc Record type. */
    nx::vms::api::AuditRecordType eventType = nx::vms::api::AuditRecordType::notDefined;

    /**%apidoc Record creation time in seconds. */
    std::chrono::seconds createdTimeS{0};

    /**%apidoc Information about the user whose actions created the record. */
    nx::network::rest::AuthSession authSession;

    /**%apidoc:any Detailed information about what's happened. */
    AllAuditDetails::type details = nullptr;

    /**%apidoc[proprietary]
     * Internal audit information about API request.
     */
    std::optional<QJsonValue> apiInfo;

    QnAuditRecord(nx::network::rest::audit::Record auditRecord):
        authSession(std::move(auditRecord.session)), apiInfo(std::move(auditRecord.apiInfo))
    {
    }

    bool operator==(const QnAuditRecord& other) const = default;

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
    static QnAuditRecord prepareRecord(
        const nx::network::rest::audit::Record& auditRecord, Details&& details = {})
    {
        static_assert(
            std::is_same_v<typename details::details_type<type, AllAuditDetails::mapping>::type,
            std::decay_t<Details>>);
        QnAuditRecord result = prepareRecord(type, auditRecord);
        result.details.emplace<std::decay_t<Details>>(std::forward<Details>(details));
        return result;
    }

    static QnAuditRecord prepareRecord(
        nx::vms::api::AuditRecordType type, const nx::network::rest::audit::Record& auditRecord);
    static void setCreatedTimeForTests(std::optional<std::chrono::seconds> value);

private:
    QnAuditRecord(): authSession{{{nx::Uuid{}}}} {}

    template<class Collection>
    friend void QJsonDetail::serialize_collection(QnJsonContext*, const Collection&, QJsonValue*);

    template<class Collection, class Element>
    friend bool QJsonDetail::deserialize_collection_element(
        QnJsonContext*, const QJsonValue&, Collection*, const Element*, const QnCollection::list_tag&);
};
#define QnAuditRecord_Fields (eventType)(createdTimeS)(authSession)(details)(apiInfo)
QN_FUSION_DECLARE_FUNCTIONS(QnAuditRecord, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(QnAuditRecord, QnAuditRecord_Fields)

struct NX_VMS_COMMON_API QnAuditRecordModel: QnAuditRecord
{
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
