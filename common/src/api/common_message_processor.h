#ifndef COMMON_MESSAGE_PROCESSOR_H
#define COMMON_MESSAGE_PROCESSOR_H

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/message_source.h>
#include <business/business_event_rule.h>

#include <utils/common/singleton.h>

class QnCommonMessageProcessor: public QObject, public Singleton<QnCommonMessageProcessor>
{
    Q_OBJECT
public:
    explicit QnCommonMessageProcessor(QObject *parent = 0);

    virtual void run();
    virtual void stop();

    virtual void init(const QUrl &url, const QString &authKey, int reconnectTimeout = EVENT_RECONNECT_TIMEOUT);

signals:
    void connectionReset();

    void businessRuleChanged(const QnBusinessEventRulePtr &rule);
    void businessRuleDeleted(int id);
    void businessRuleReset(QnBusinessEventRuleList rules);

    void businessActionReceived(const QnAbstractBusinessActionPtr& action);

protected:
    virtual void handleMessage(const QnMessage &message);

protected:
    QSharedPointer<QnMessageSource> m_source;

    static const int EVENT_RECONNECT_TIMEOUT = 3000;
};

#endif // COMMON_MESSAGE_PROCESSOR_H
