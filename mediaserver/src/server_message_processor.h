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

    void init(const QUrl& url, const QByteArray& authKey, int reconnectTimeout);
    void stop();

signals:
    void connectionOpened();
    void connectionReset();
    void businessRuleChanged(QnBusinessEventRulePtr bEvent);
    void businessRuleDeleted(int id);
    void businessRuleReset(QnBusinessEventRuleList rules);
    void businessActionReceived(QnAbstractBusinessActionPtr bAction);
public slots:
    void run();

private slots:
    void at_messageReceived(QnMessage message);
    void at_connectionOpened(QnMessage message);
    void at_connectionClosed(QString errorString);
    void at_connectionReset();

private:
    QSharedPointer<QnMessageSource> m_source;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
