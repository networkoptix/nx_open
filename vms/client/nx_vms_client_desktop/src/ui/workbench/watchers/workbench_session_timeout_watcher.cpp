#include "workbench_session_timeout_watcher.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <ui/workbench/handlers/workbench_connect_handler.h>

#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using LogicalState = QnWorkbenchConnectHandler::LogicalState;
using PhysicalState = QnWorkbenchConnectHandler::PhysicalState;
using DisconnectFlag = QnWorkbenchConnectHandler::DisconnectFlag;

WorkbenchSessionTimeoutWatcher::WorkbenchSessionTimeoutWatcher(QnWorkbenchConnectHandler* owner):
    QObject(owner)
{
    static const int kMsecPerMinute = 60 * 1000;

    auto sessionTimeoutTimer = new QTimer(this);
    sessionTimeoutTimer->setInterval(kMsecPerMinute); //< One minute precision is quite enough.
    connect(sessionTimeoutTimer, &QTimer::timeout, this,
        [this, owner]
        {
            if (owner->logicalState() != LogicalState::connected)
                return;

            const auto sessionTimeoutMinutes = owner->globalSettings()->sessionTimeoutLimit()
                .count();

            if (sessionTimeoutMinutes <= 0)
                return;

            const auto minutesPassed =
                (qnSyncTime->currentMSecsSinceEpoch() - m_connectedAtMsecSinceEpoch)
                / kMsecPerMinute;

            if (minutesPassed >= sessionTimeoutMinutes)
                owner->disconnectFromServer( DisconnectFlag::Force | DisconnectFlag::SessionTimeout);
        });

    const auto handleConnectionStateChanged =
        [this, sessionTimeoutTimer](LogicalState logicalValue, PhysicalState /*physicalValue*/)
        {
            const bool wasConnected = m_connectedAtMsecSinceEpoch > 0;
            const bool isConnected = logicalValue == LogicalState::connected;
            if (isConnected == wasConnected)
                return;

            if (isConnected)
            {
                m_connectedAtMsecSinceEpoch = qnSyncTime->currentMSecsSinceEpoch();
                sessionTimeoutTimer->start();
            }
            else
            {
                m_connectedAtMsecSinceEpoch = 0;
                sessionTimeoutTimer->stop();
            }
        };

    connect(owner, &QnWorkbenchConnectHandler::stateChanged, this,
        handleConnectionStateChanged);
}

} // namespace nx::vms::client::desktop
