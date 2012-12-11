#ifndef __BUSINESS_MESSAGE_BUS_H_
#define __BUSINESS_MESSAGE_BUS_H_

#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMap>
#include <QUrl>
#include <QThread>
#include "abstract_business_event.h"
#include "abstract_business_action.h"

/*
* High level business message transport.
* Business event should be sended to this class as standard QT signal.
* This class check event/action rules and send businessAction to destination
*/

class QnBusinessMessageBus: public QThread
{
    Q_OBJECT
public:
    QnBusinessMessageBus();

    static QnBusinessMessageBus* instance();

    //int deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url);

    /** Delivery action to other module */
    int deliveryBusinessAction(QnAbstractBusinessActionPtr bAction, const QUrl& url);
signals:
    /** Action successfully delivered to other module*/
    void actionDelivered(QnAbstractBusinessActionPtr action);

    /** Fail to delivery action to other module*/
    void actionDeliveryFail(QnAbstractBusinessActionPtr action);

    /** Action received from other module */
    void actionReceived(QnAbstractBusinessActionPtr action);
public slots:
    /** Action received from other module */
    void at_actionReceived(QnAbstractBusinessActionPtr action);
private slots:
    void at_replyFinished(QNetworkReply* reply);
    void at_replyError(QNetworkReply::NetworkError code);
private:
    QNetworkAccessManager m_transport;
    typedef QMap<QNetworkReply*, QnAbstractBusinessActionPtr> ActionMap;
    ActionMap m_actionsInProgress;
    mutable QMutex m_mutex;
};

#define qnBusinessMessageBus QnBusinessMessageBus::instance()

#endif // __BUSINESS_MESSAGE_BUS_H_
