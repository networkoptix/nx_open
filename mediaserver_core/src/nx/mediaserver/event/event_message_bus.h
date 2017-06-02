#pragma once

#include <QtCore/QMap>
#include <QtCore/QUrl>
#include <QtCore/QThread>

#include <common/common_module_aware.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/actions/abstract_action.h>

class QnApiSerializer;

namespace nx {
namespace mediaserver {
namespace event {

/*
* High level event message transport.
* Event should be sent to this class as standard QT signal.
* The class checks event/action rules and sends event action to destination
*/

class EventMessageBus:
    public QThread,
    public Singleton<EventMessageBus>,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    EventMessageBus(QnCommonModule* commonModule);
    virtual ~EventMessageBus() override;

    //int deliveryBusinessEvent(QnAbstractBusinessEventPtr bEvent, const QUrl& url);

    /** Deliver action to other module */
    int deliverAction(const vms::event::AbstractActionPtr& action, const QnUuid& dstPeer);

signals:
    /** Action successfully delivered to other module*/
    void actionDelivered(const nx::vms::event::AbstractActionPtr& action);

    /** Fail to delivery action to other module*/
    void actionDeliveryFail(const nx::vms::event::AbstractActionPtr& action);

    /** Action received from other module */
    void actionReceived(const nx::vms::event::AbstractActionPtr& action);

public slots:
    /** Action received from other module */
    void at_actionReceived(const vms::event::AbstractActionPtr& action);

private slots:
    void at_DeliverActionFinished(int handle, ec2::ErrorCode errorCode);

private:
    mutable QnMutex m_mutex;
    QMap<int, vms::event::AbstractActionPtr> m_sendingActions;
};

#define qnEventMessageBus nx::mediaserver::event::EventMessageBus::instance()

} // namespace event
} // namespace mediaserver
} // namespace nx
