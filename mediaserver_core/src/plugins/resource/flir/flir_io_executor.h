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
    IoExecutor(QObject* parent = 0);
    virtual ~IoExecutor();
    QThread* getThread() const;
private:
    const std::unique_ptr<QThread> m_thread;
};

} // namespace flir
} // namespace plugins
} // nx