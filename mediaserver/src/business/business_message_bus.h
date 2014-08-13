#ifndef __BUSINESS_MESSAGE_BUS_H_
#define __BUSINESS_MESSAGE_BUS_H_

#include <QtCore/QMutex>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QMap>
#include <QtCore/QUrl>
#include <QtCore/QThread>

#include <business/events/abstract_business_event.h>
#include <business/actions/abstract_business_action.h>
#include "nx_ec/impl/ec_api_impl.h"

class QnApiSerializer;

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
    virtual ~QnBusinessMessageBus();

    static QnBusinessMessageBus* instance();

    //int deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url);

    /** Delivery action to other module */
    int deliveryBusinessAction(const QnAbstractBusinessActionPtr &bAction, const QUuid& dstPeer);

signals:
    /** Action successfully delivered to other module*/
    void actionDelivered(const QnAbstractBusinessActionPtr &action);

    /** Fail to delivery action to other module*/
    void actionDeliveryFail(const QnAbstractBusinessActionPtr &action);

    /** Action received from other module */
    void actionReceived(const QnAbstractBusinessActionPtr &action);

public slots:
    /** Action received from other module */
    void at_actionReceived(const QnAbstractBusinessActionPtr &action);

private slots:
    void at_DeliveryBusinessActionFinished( int handle, ec2::ErrorCode errorCode );

private:
    QNetworkAccessManager m_transport;
    typedef QMap<QNetworkReply*, QnAbstractBusinessActionPtr> ActionMap;
    ActionMap m_actionsInProgress;
    mutable QMutex m_mutex;
    QMap<int, QnAbstractBusinessActionPtr> m_sendingActions;
};

#define qnBusinessMessageBus QnBusinessMessageBus::instance()

#endif // __BUSINESS_MESSAGE_BUS_H_
