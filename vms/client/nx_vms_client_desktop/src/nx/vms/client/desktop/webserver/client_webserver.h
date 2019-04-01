#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

class QJsonDocument;
class QtHttpServer;
class QtHttpReply;
class QtHttpRequest;

namespace nx::vmx::client::desktop {

class DirectorJsonInterface;

/**
* DirectorWebserver provides HTTP API for operational control and telemetry for NX client.
* Uses DirectorJsonInterface to take actions.
* It is used (or to be used) for functional testing.
*/

class DirectorWebserver: public QObject
{
    Q_OBJECT

    static constexpr int kDefaultPort = 7012;
public:
    explicit DirectorWebserver(QObject* parent);
    virtual ~DirectorWebserver();

    void setPort(int port);

    bool start();
    void stop();

private:
    void makeReply(QtHttpRequest* request, QtHttpReply* reply);

    QScopedPointer<QtHttpServer> m_server;
    QScopedPointer<DirectorJsonInterface> m_directorInterface;
    int m_port = kDefaultPort;
    bool m_active = false;
};

} // namespace nx::vmx::client::desktop
