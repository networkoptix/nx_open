#pragma once

#include <QtWebSockets/QWebSocket>

class FlirWebSocketProxy: public QObject
{
    Q_OBJECT

public:
    FlirWebSocketProxy();

public:
    QWebSocket* getSocket();
    void open(const QUrl& url);
    void sendTextMessage(const QString& message);

private slots:
    void openInternal(const QUrl& url);
    void sendTextMessageInternal(const QString& message);

private:
    QObject* m_parent;
    QWebSocket* m_socket;
};