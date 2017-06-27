#include "business_message_bus.h"

#include <QtCore/QUrlQuery>

#include <nx/fusion/serialization/binary.h>

#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include "api/app_server_connection.h"
#include <common/common_module.h>

QnBusinessMessageBus::QnBusinessMessageBus(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

QnBusinessMessageBus::~QnBusinessMessageBus()
{
    const int kStopTimeoutMs = 5000;

    quit();
    if (!wait(kStopTimeoutMs))
    {
        terminate();
        wait();
    }
}

/*
int QnBusinessMessageBus::deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url)
{
    return 0;
}
*/

int QnBusinessMessageBus::deliveryBusinessAction(const QnAbstractBusinessActionPtr &bAction, const QnUuid& dstPeer)
{
    ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();
    int handle = ec2Connection->getBusinessEventManager(Qn::kSystemAccess)->sendBusinessAction(bAction, dstPeer, this, &QnBusinessMessageBus::at_DeliveryBusinessActionFinished);

    QnMutexLocker lock(&m_mutex);
    m_sendingActions.insert(handle, bAction);
    return 0;
}

void QnBusinessMessageBus::at_DeliveryBusinessActionFinished( int handle, ec2::ErrorCode errorCode )
{
    QnAbstractBusinessActionPtr action;
    {
        QnMutexLocker lock(&m_mutex);
        action = m_sendingActions.value(handle);
        m_sendingActions.remove(handle);
    }

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
