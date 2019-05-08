#include "web_channel_server.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/QWebSocket>
#include <QtWebChannel/QWebChannelAbstractTransport>

#include <utils/common/app_info.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mobile_client {
namespace webchannel {

class WebSocketTransport: public QWebChannelAbstractTransport
{
public:
    WebSocketTransport(QWebSocket* socket, QObject* parent):
        QWebChannelAbstractTransport(parent),
        m_socket(socket)
    {
        connect(socket, &QWebSocket::textMessageReceived,
            this, &WebSocketTransport::textMessageReceived);
        connect(socket, &QWebSocket::disconnected,
            this, &WebSocketTransport::deleteLater);
    }

    virtual ~WebSocketTransport() override
    {
        m_socket->deleteLater();
    }

    virtual void sendMessage(const QJsonObject& message) override
    {
        QJsonDocument document(message);
        m_socket->sendTextMessage(QString::fromUtf8(document.toJson(QJsonDocument::Compact)));
    }

private:
    void textMessageReceived(const QString& messageData)
    {
        QJsonParseError error;
        QJsonDocument message = QJsonDocument::fromJson(messageData.toUtf8(), &error);
        if (error.error)
        {
            NX_ERROR(this, "Failed to parse text message as JSON object: %1", error.errorString());
            return;
        }
        else if (!message.isObject())
        {
            NX_ERROR(this, "Received JSON message that is not an object: %1", messageData);
            return;
        }
        emit messageReceived(message.object(), this);
    }

private:
    QWebSocket* m_socket = nullptr;
};

WebChannelServer::WebChannelServer(const quint16 port, QObject* parent):
    QWebChannel(parent),
    m_server(new QWebSocketServer(
        QString("%1 WebChannel Server").arg(QnAppInfo::productNameLong()),
        QWebSocketServer::NonSecureMode))
{
    if (!m_server->listen(QHostAddress::LocalHost, port))
    {
        NX_ERROR(this, "Could not start server");

        delete m_server;
        m_server = nullptr;
        return;
    }

    NX_DEBUG(this, "Server started at localhost: %1", m_server->serverPort());

    connect(m_server, &QWebSocketServer::newConnection, this,
        [this]
        {
            connectTo(new WebSocketTransport(m_server->nextPendingConnection(), this));
        });
}

bool WebChannelServer::isValid() const
{
    return m_server;
}

quint16 WebChannelServer::serverPort() const
{
    return m_server ? m_server->serverPort() : 0;
}

} // namespace webchannel
} // namespace mobile_client
} // namespace nx
