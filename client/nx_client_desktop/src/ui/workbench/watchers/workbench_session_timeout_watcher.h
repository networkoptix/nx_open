#pragma once

#include <QtCore/QObject>

class QnWorkbenchConnectHandler;

namespace nx::client::desktop {

class WorkbenchSessionTimeoutWatcher: public QObject
{
public:
    WorkbenchSessionTimeoutWatcher(QnWorkbenchConnectHandler* owner);

private:
    qint64 m_connectedAtMsecSinceEpoch = 0;
};

} // namespace nx::client::desktop
