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

class ServerResource;
using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;

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

    static QTimeZone timeZone(const QnMediaResourcePtr& resource);

    /** Time value, than takes into account user time mode settings. */
    QDateTime displayTime(qint64 msecsSinceEpoch) const; //< Using current server.
    QDateTime displayTime(const ServerResourcePtr& server, qint64 msecsSinceEpoch) const;

signals:
    void timeZoneChanged();

private:
    void ensureServerTimeZoneIsValid(const ServerResourcePtr& server);

private:
    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);

private:
    TimeMode m_mode = serverTimeMode;
    QTimer m_timer;
};

} // namespace nx::vms::client::core
