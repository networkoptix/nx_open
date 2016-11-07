#pragma once

#include <set>

#include <QtCore/QThread>
#include <QtWebSockets/QWebSocket>

#include <nx/utils/singleton.h>

class FlirIOExecutor: 
    public QObject,
    public Singleton<FlirIOExecutor>
{
    Q_OBJECT
public:
    FlirIOExecutor();
    virtual ~FlirIOExecutor();
    QThread* getThread() const;
private:
    QThread* m_thread;
};