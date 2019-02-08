#pragma once

#include <QtCore/QObject>

class QnWorkbenchConnectHandler;

namespace nx::vms::client::desktop {

class WorkbenchSessionTimeoutWatcher: public QObject
{
public:
    WorkbenchSessionTimeoutWatcher(QnWorkbenchConnectHandler* owner);

private:
    qint64 m_connectedAtMsecSinceEpoch = 0;
};

} // namespace nx::vms::client::desktop
