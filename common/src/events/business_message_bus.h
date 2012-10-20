#ifndef __BUSINESS_MESSAGE_BUS_H_
#define __BUSINESS_MESSAGE_BUS_H_

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
    int deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, QnResourcePtr destination);
    int deliveryBusinessAction(QnAbstractBusinessActionPtr bAction, QnResourcePtr destination);
public slots:
signals:
    void businessEventDelivered(int deliveryNum);
    void businessActionDelivered(int deliveryNum);
};

#endif // __BUSINESS_MESSAGE_BUS_H_
