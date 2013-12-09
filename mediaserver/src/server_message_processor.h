#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <QSharedPointer>

#include "api/message_source.h"
#include <api/common_message_processor.h>

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();

    virtual void init(const QUrl &url, const QString &authKey, int reconnectTimeout = EVENT_RECONNECT_TIMEOUT) override;
signals:
    void connectionOpened();
    void connectionReset();
    void businessRuleChanged(QnBusinessEventRulePtr bEvent);
    void businessRuleDeleted(int id);
    void businessRuleReset(QnBusinessEventRuleList rules);
    void businessActionReceived(QnAbstractBusinessActionPtr bAction);

private slots:
    void at_messageReceived(QnMessage message);
    void at_connectionOpened(QnMessage message);
    void at_connectionClosed(QString errorString);
    void at_connectionReset();

private:
    bool m_tryDirectConnect;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
