#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <QSharedPointer>

#include "api/message_source.h"

class QnServerMessageProcessor : public QObject
{
    Q_OBJECT

public:
    static QnServerMessageProcessor* instance();

    QnServerMessageProcessor();

    void init(const QUrl& url, int reconnectTimeout);
    void stop();

signals:
    void connectionReset();

public slots:
    void run();

private slots:
    void at_messageReceived(QnMessage event);
    void at_connectionClosed(QString errorString);
    void at_connectionReset();

private:
    QSharedPointer<QnMessageSource> m_source;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
