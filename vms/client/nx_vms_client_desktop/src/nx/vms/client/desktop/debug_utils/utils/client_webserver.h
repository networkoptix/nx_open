// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QHostAddress>

class QJsonDocument;
class QtHttpServer;
class QtHttpReply;
class QtHttpRequest;

namespace nx::vms::client::desktop {

class Director;
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
    explicit DirectorWebserver(Director* director, QObject* parent = nullptr);
    virtual ~DirectorWebserver();

    /** Changes listen address for the server.*/
    bool setListenAddress(const QString& host, int port);

    bool start();
    void stop();

private:
    /** Called by QnHttpServer when new client connects. */
    void onClientConnected(const QString & guid);
    /** Called by QnHttpServer when client disconnects. */
    void onClientDisconnected(const QString & guid);
    /** Called by QnHttpServer when something goes wrong. */
    void onServerError(const QString& errorMessage);

    /** Called every time QnHttpServer gets a request. */
    void processRequest(QtHttpRequest* request, QtHttpReply* reply);

    QScopedPointer<QtHttpServer> m_server;
    QScopedPointer<DirectorJsonInterface> m_directorInterface;

    /** Listen address. */
    QHostAddress m_address;
    /** Listen port. */
    int m_port = kDefaultPort;

    bool m_active = false;
};

} // namespace nx::vms::client::desktop
