#include "business_message_bus.h"

#include <QtCore/QUrlQuery>

#include <utils/serialization/binary.h>

#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include "api/app_server_connection.h"

Q_GLOBAL_STATIC(QnBusinessMessageBus, QnBusinessMessageBus_instance)

QnBusinessMessageBus* QnBusinessMessageBus::instance()
{
    QnBusinessMessageBus* _instance = QnBusinessMessageBus_instance();
    if( !_instance->isRunning() )
        _instance->start();
    return _instance;
}

QnBusinessMessageBus::QnBusinessMessageBus()
{
}

QnBusinessMessageBus::~QnBusinessMessageBus()
{
    quit();
    wait();
}

/*
int QnBusinessMessageBus::deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url)
{
    return 0;
}
*/

int QnBusinessMessageBus::deliveryBusinessAction(const QnAbstractBusinessActionPtr &bAction, const QnUuid& dstPeer)
{
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    int handle = ec2Connection->getBusinessEventManager()->sendBusinessAction(bAction, dstPeer, this, &QnBusinessMessageBus::at_DeliveryBusinessActionFinished);
    m_sendingActions.insert(handle, bAction);
    return 0;
}

void QnBusinessMessageBus::at_DeliveryBusinessActionFinished( int handle, ec2::ErrorCode errorCode )
{
    QnAbstractBusinessActionPtr action = m_sendingActions.value(handle);
    m_sendingActions.remove(handle);
    if (errorCode == ec2::ErrorCode::ok)
        emit actionDelivered(action);
    else {
        qWarning() << "error delivering exec action message #" << handle << "error:" << ec2::toString(errorCode);
        emit actionDeliveryFail(action);
    }
}

void QnBusinessMessageBus::at_actionReceived(const QnAbstractBusinessActionPtr &action)
{
    emit actionReceived(action);
}
