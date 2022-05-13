// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/system_context_aware.h>

struct QnTimeReply;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ServerTimeWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    ServerTimeWatcher(SystemContext* systemContext, QObject* parent = nullptr);

    enum TimeMode
    {
        serverTimeMode,
        clientTimeMode
    };

    TimeMode timeMode() const;
    void setTimeMode(TimeMode mode);

    /** Utc offset for the given resource. Supports cameras (current parent server offset is used)
     *  and exported files (offset is stored inside).
     */
    static qint64 utcOffset(const QnMediaResourcePtr& resource,
        qint64 defaultValue = Qn::InvalidUtcOffset);

    /** Offset value, used in gui elements. Depends on user time mode settings. */
    qint64 displayOffset(const QnMediaResourcePtr& resource) const;

    /** Current time on the given server. Really, sync time is used, but with correct utc offset. */
    static QDateTime serverTime(const QnMediaServerResourcePtr& server, qint64 msecsSinceEpoch);

    /** Time value, than takes into account user time mode settings. */
    QDateTime displayTime(qint64 msecsSinceEpoch) const; //< Using current server.
    QDateTime displayTime(const QnMediaServerResourcePtr& server, qint64 msecsSinceEpoch) const;

signals:
    void displayOffsetsChanged();
    void timeModeChanged();

private:
    void sendRequest(const QnMediaServerResourcePtr& server);

    qint64 localOffset(const QnMediaResourcePtr& resource,
        qint64 defaultValue = Qn::InvalidUtcOffset) const;

private:
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

private:
    TimeMode m_mode = serverTimeMode;
    QTimer m_timer;
};

} // namespace nx::vms::client::core
