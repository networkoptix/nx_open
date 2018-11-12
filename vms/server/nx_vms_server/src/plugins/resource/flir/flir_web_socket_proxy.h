#pragma once

#include <QtWebSockets/QWebSocket>

namespace nx {
namespace plugins {
namespace flir {
namespace nexus {

class WebSocketProxy: public QObject
{
    Q_OBJECT

public:
    WebSocketProxy(QObject* parent = nullptr);

public:
    QWebSocket* getSocket();
    void open(const QUrl& url);
    void sendTextMessage(const QString& message);

private slots:
    void openInternal(const QUrl& url);
    void sendTextMessageInternal(const QString& message);

private:
    QWebSocket* m_socket;
};

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx