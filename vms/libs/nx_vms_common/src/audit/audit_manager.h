// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/audit.h>
#include <nx/vms/api/data/audit.h>

#include "audit_manager_fwd.h"

class NX_VMS_COMMON_API QnAuditManager: public nx::network::rest::audit::Manager
{
public:
    virtual ~QnAuditManager() = default;

    static const int MIN_PLAYBACK_TIME_TO_LOG = 1000 * 5;
    static const int AGGREGATION_TIME_MS = 1000 * 5;
    static const qint64 MIN_SEEK_DISTANCE_TO_LOG = 1000 * 60;

    /* notify new playback was started from position timestamp
    *  return internal ID of started session
    */
    virtual AuditHandle notifyPlaybackStarted(
        const nx::network::rest::audit::Record& auditRecord,
        const nx::Uuid& id,
        qint64 timestampUsec,
        bool isExport = false) = 0;

    virtual void notifyPlaybackInProgress(const AuditHandle& handle, qint64 timestampUsec) = 0;
    virtual void notifySettingsChanged(const nx::network::rest::audit::Record& auditRecord,
        std::map<QString, QString> settings) = 0;

    virtual AuditHandle notifyProxySessionStarted(
        const nx::network::rest::audit::Record& auditRecord,
        const nx::utils::Url& dstUrl,
        bool wasConnected) = 0;

    template <nx::vms::api::AuditRecordType type,
        typename Details = typename nx::vms::api::details::details_type<type,
            nx::vms::api::AllAuditDetails::mapping>::type>
    void addAuditRecord(const nx::network::rest::audit::Record& auditRecord, Details&& details = {})
    {
        return addAuditRecord(nx::vms::api::AuditRecord::prepareRecord<type>(
            auditRecord, createdTime(), std::forward<Details>(details)));
    }

    virtual void addAuditRecord(const nx::network::rest::audit::Record& auditRecord) override
    {
        return addAuditRecord(
            nx::vms::api::AuditRecord::prepareRecord<nx::vms::api::AuditRecordType::notDefined>(
                auditRecord, createdTime()));
    }

    virtual void addAuditRecord(const nx::vms::api::AuditRecord& record) = 0;
    virtual void flushAuditRecords() = 0;

    virtual void at_connectionOpened(const nx::network::rest::audit::Record& auditRecord) = 0;
    virtual void at_connectionClosed(const nx::network::rest::audit::Record& auditRecord) = 0;
    virtual void stop() = 0;

    static std::chrono::seconds createdTime();
    static void setCreatedTimeForTests(std::optional<std::chrono::seconds> value);
};
