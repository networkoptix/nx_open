#ifndef __BUSINESS_MESSAGE_BUS_H_
#define __BUSINESS_MESSAGE_BUS_H_

#include <QUrl>
#include <QThread>
#include "abstract_business_event.h"
#include "abstract_business_action.h"
#include "core/resource/resource_fwd.h"

/*
* High level business message transport.
* Business event should be sended to this class as standard QT signal.
* This class check event/action rules and send businessAction to destination
*/

class QnBusinessMessageBus: public QThread
{
public:
    int deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url);
    int deliveryBusinessAction(QnAbstractBusinessActionPtr bAction, const QUrl& url);
public slots:
signals:
    void businessEventDelivered(int deliveryNum);
    void businessActionDelivered(int deliveryNum);
};

#endif // __BUSINESS_MESSAGE_BUS_H_
