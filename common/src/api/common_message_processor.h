#ifndef COMMON_MESSAGE_PROCESSOR_H
#define COMMON_MESSAGE_PROCESSOR_H

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/model/kvpair.h>

#include <business/business_fwd.h>

#include <utils/common/singleton.h>

class QnMessage;
class QnMessageSource;

class QnCommonMessageProcessor: public QObject, public Singleton<QnCommonMessageProcessor>
{
    Q_OBJECT
public:
    explicit QnCommonMessageProcessor(QObject *parent = 0);
    virtual ~QnCommonMessageProcessor() {}

    virtual void run();
    virtual void stop();

    virtual void init(const QUrl &url, const QString &authKey, int reconnectTimeout = EVENT_RECONNECT_TIMEOUT);

signals:
    void connectionOpened();
    void connectionClosed();
    void connectionReset();

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

    void businessRuleChanged(const QnBusinessEventRulePtr &rule);
    void businessRuleDeleted(int id);
    void businessRuleReset(QnBusinessEventRuleList rules);

    void businessActionReceived(const QnAbstractBusinessActionPtr& action);
protected:
    virtual void loadRuntimeInfo(const QnMessage &message);
    virtual void handleConnectionOpened(const QnMessage &message);
    virtual void handleConnectionClosed(const QString &errorString);
    virtual void handleMessage(const QnMessage &message);
    virtual void updateKvPairs(const QnKvPairListsById &kvPairs);

private slots:
    void at_connectionOpened(const QnMessage &message);
    void at_connectionClosed(const QString &errorString);
    void at_messageReceived(const QnMessage &message);

protected:
    QSharedPointer<QnMessageSource> m_source;

    static const int EVENT_RECONNECT_TIMEOUT = 3000;

private:
    QUrl m_url;
    QString m_authKey;
    int m_reconnectTimeout;
};

#endif // COMMON_MESSAGE_PROCESSOR_H
