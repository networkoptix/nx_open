#pragma once

#include <set>

#include <QtCore/QThread>
#include <QtWebSockets/QWebSocket>

#include <nx/utils/singleton.h>

class FlirIoExecutor: 
    public QObject,
    public Singleton<FlirIoExecutor>
{
    Q_OBJECT
public:
    FlirIoExecutor();
    virtual ~FlirIoExecutor();
    QThread* getThread() const;
private:
    std::unique_ptr<QThread> m_thread;
};