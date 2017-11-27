#include "event_message_bus.h"

#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <common/common_module.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <nx/fusion/serialization/binary.h>

namespace nx {
namespace mediaserver {
namespace event {

EventMessageBus::EventMessageBus(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

EventMessageBus::~EventMessageBus()
{
    const int kStopTimeoutMs = 5000;

    quit();
    if (!wait(kStopTimeoutMs))
    {
        terminate();
        wait();
    }
}

int EventMessageBus::deliverAction(const vms::event::AbstractActionPtr&action, const QnUuid& dstPeer)
{
    ec2::AbstractECConnectionPtr ec2Connection = commonModule()->ec2Connection();
    int handle = ec2Connection->getBusinessEventManager(Qn::kSystemAccess)->sendBusinessAction(
        action, dstPeer, this, &EventMessageBus::at_DeliverActionFinished);

    QnMutexLocker lock(&m_mutex);
    m_sendingActions.insert(handle, action);
    return 0;
}

void EventMessageBus::at_DeliverActionFinished(int handle, ec2::ErrorCode errorCode)
{
    QnMutexLocker lock(&m_mutex);
    auto action = m_sendingActions.value(handle);
    m_sendingActions.remove(handle);
    lock.unlock();

    if (errorCode == ec2::ErrorCode::ok)
    {
        emit actionDelivered(action);
    }
    else
    {
        qWarning() << "error delivering exec action message #" << handle << "error:" << ec2::toString(errorCode);
        emit actionDeliveryFail(action);
    }
}

void EventMessageBus::at_actionReceived(const vms::event::AbstractActionPtr& action)
{
    emit actionReceived(action);
}

} // namespace event
} // namespace mediaserver
} // namespace nx
