// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <deque>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <api/model/audit/audit_record.h>
#include <api/model/audit/auth_session.h>
#include <nx/utils/thread/mutex.h>
#include <recording/time_period.h>

#include "audit_manager_fwd.h"

namespace Qn { struct UserAccessData; }

class NX_VMS_COMMON_API QnAuditManager
{
public:
    virtual ~QnAuditManager() = default;

    static const int MIN_PLAYBACK_TIME_TO_LOG = 1000 * 5;
    static const int AGGREGATION_TIME_MS = 1000 * 5;
    static const qint64 MIN_SEEK_DISTANCE_TO_LOG = 1000 * 60;

    static QnAuditRecord prepareRecord(const QnAuthSession& authInfo, Qn::AuditRecordType recordType);

    /* notify new playback was started from position timestamp
    *  return internal ID of started session
    */
    virtual AuditHandle notifyPlaybackStarted(const QnAuthSession& session, const QnUuid& id, qint64 timestampUsec, bool isExport = false) = 0;
    virtual void notifyPlaybackInProgress(const AuditHandle& handle, qint64 timestampUsec) = 0;
    virtual void notifySettingsChanged(const QnAuthSession& authInfo, std::map<QString, QString> settings) = 0;

    /* return internal id of inserted record. Returns <= 0 if error */
    virtual int addAuditRecord(const QnAuditRecord& record) = 0;
    virtual int updateAuditRecord(int internalId, const QnAuditRecord& record) = 0;
    virtual void flushAuditRecords() = 0;
    virtual QnTimePeriod playbackRange(const AuditHandle& handle) const = 0;

    virtual void at_connectionOpened(const QnAuthSession& session, const Qn::UserAccessData& accessRights) = 0;
    virtual void at_connectionClosed(const QnAuthSession& session) = 0;
    virtual void stop() = 0;
};
