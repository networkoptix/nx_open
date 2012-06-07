#ifndef QN_CLIENT_MESSAGE_PROCESSOR_H
#define QN_CLIENT_MESSAGE_PROCESSOR_H

#include <QSharedPointer>

#include "api/message_source.h"
#include "api/app_server_connection.h"
#include "core/resource/resource.h"

class QnClientMessageProcessor : public QObject
{
    Q_OBJECT

public:
    static QnClientMessageProcessor *instance();

    QnClientMessageProcessor();

    void stop();

signals:
    void connectionOpened();
    void connectionClosed();

public slots:
    void run();

private slots:
    void at_resourcesReceived(int status, const QByteArray& errorString, QnResourceList resources, int handle);
    void at_licensesReceived(int status, const QByteArray& errorString, QnLicenseList licenses, int handle);

    void at_messageReceived(QnMessage event);
    void at_connectionClosed(QString errorString);
    void at_connectionOpened();
    void at_connectionReset();

private:
    void init();
    void init(const QUrl& url, int reconnectTimeout);

private:
    static const int EVENT_RECONNECT_TIMEOUT = 3000;

    quint32 m_seqNumber;
    QSharedPointer<QnMessageSource> m_source;
};

#endif // _client_event_manager_h
