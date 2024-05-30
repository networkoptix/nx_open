// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/rest/audit.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/qnbytearrayref.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/uuid.h>

#include "audit_details.h"

namespace nx::network::rest::json {

template<typename T>
QJsonValue defaultValue();

} // namespace nx::network::rest::json

namespace QnCollection { struct list_tag; }

namespace QJsonDetail {

template<class Collection>
void serialize_collection(QnJsonContext*, const Collection&, QJsonValue*);

template<class Collection, class Element>
bool deserialize_collection_element(
    QnJsonContext*, const QJsonValue&, Collection*, const Element*, const QnCollection::list_tag&);

} // namespace QJsonDetail

namespace nx::vms::api {

struct NX_VMS_API AuditLogFilter
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
QN_FUSION_DECLARE_FUNCTIONS(AuditLogFilter, (json), NX_VMS_API)

struct NX_VMS_API AuditRecord
{
    nx::Uuid serverId;

    /**%apidoc Record type. */
    AuditRecordType eventType = AuditRecordType::notDefined;

    /**%apidoc Record creation time in seconds. */
    std::chrono::seconds createdTimeS{0};

    /**%apidoc Information about the user whose actions created the record. */
    nx::network::rest::AuthSession authSession;

    /**%apidoc
     * Detailed information about what's happened.
     * %// The `details` declaration below should be a single line for apidoctool processing.
     */
    std::variant<std::nullptr_t, DeviceDetails, PlaybackDetails, SessionDetails, ResourceDetails, DescriptionDetails, UpdateDetails, MitmDetails> details = nullptr;

    /**%apidoc[proprietary]
     * Internal audit information about API request.
     */
    std::optional<QJsonValue> apiInfo;

    AuditRecord(nx::network::rest::audit::Record auditRecord):
        authSession(std::move(auditRecord.session)), apiInfo(std::move(auditRecord.apiInfo))
    {
    }

    bool operator==(const AuditRecord& other) const = default;

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

    template<AuditRecordType type,
        typename Details = typename details::details_type<type, AllAuditDetails::mapping>::type>
    static AuditRecord prepareRecord(
        const nx::network::rest::audit::Record& auditRecord,
        std::chrono::seconds createdTime,
        Details&& details = {})
    {
        static_assert(
            std::is_same_v<typename details::details_type<type, AllAuditDetails::mapping>::type,
            std::decay_t<Details>>);
        AuditRecord result = prepareRecord(type, auditRecord, createdTime);
        result.details.emplace<std::decay_t<Details>>(std::forward<Details>(details));
        return result;
    }

    static AuditRecord prepareRecord(
        AuditRecordType type,
        const nx::network::rest::audit::Record& auditRecord,
        std::chrono::seconds createdTime);

private:
    AuditRecord(): authSession{{{nx::Uuid{}}}} {}

    template<typename T>
    friend QJsonValue nx::network::rest::json::defaultValue();

    template<class Collection>
    friend void QJsonDetail::serialize_collection(QnJsonContext*, const Collection&, QJsonValue*);

    template<class Collection, class Element>
    friend bool QJsonDetail::deserialize_collection_element(
        QnJsonContext*, const QJsonValue&, Collection*, const Element*, const QnCollection::list_tag&);
};
#define AuditRecord_Fields (serverId)(eventType)(createdTimeS)(authSession)(details)(apiInfo)
QN_FUSION_DECLARE_FUNCTIONS(AuditRecord, (ubjson)(json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(AuditRecord, AuditRecord_Fields)
NX_REFLECTION_TAG_TYPE(AuditRecord, jsonSerializeChronoDurationAsNumber)

using AuditRecordList = std::vector<AuditRecord>;

} // namespace nx::vms::api
