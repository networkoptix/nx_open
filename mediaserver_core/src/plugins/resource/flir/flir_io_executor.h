#pragma once

#include <set>

#include <QtCore/QThread>
#include <QtWebSockets/QWebSocket>

#include <nx/utils/singleton.h>

namespace nx {
namespace plugins {
namespace flir {

class IoExecutor: 
    public QObject,
    public Singleton<IoExecutor>
{
    Q_OBJECT
public:
    IoExecutor();
    virtual ~IoExecutor();
    QThread* getThread() const;
private:
    std::unique_ptr<QThread> m_thread;
};

} // namespace flir
} // namespace plugins
} // nx