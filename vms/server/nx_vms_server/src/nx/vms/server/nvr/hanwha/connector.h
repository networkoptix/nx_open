#pragma once

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <api/model/api_ioport_data.h>

#include <nx/utils/uuid.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/api/data/network_block_data.h>

#include <nx/vms/server/nvr/types.h>

class QnResourcePool;

namespace nx::vms::server::event { class EventConnector; }

namespace nx::vms::server::nvr {

class INetworkBlockManager;
class IIoManager;
class IBuzzerManager;
class IFanManager;
class ILedManager;

} // namespace nx::vms::server::nvr

namespace nx::vms::server::nvr::hanwha {

class Connector: public Connective<QObject>
{
    Q_OBJECT;
public:
    Connector(
        const QnMediaServerResourcePtr server,
        nx::vms::server::event::EventConnector* eventConnector,
        INetworkBlockManager* networkBlockManager,
        IIoManager* ioManager,
        IBuzzerManager* buzzerManager,
        IFanManager* fanManager,
        ILedManager* ledManager);

    virtual ~Connector();

signals:
    void poeOverBudget(const nx::vms::event::PoeOverBudgetEventPtr& event);
    void fanError(const nx::vms::event::FanErrorEventPtr& event);

public slots:
    void at_resourceStatusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);
    void at_resourceAddedOrRemoved(const QnResourcePtr& resource);

private:
    void handlePoeOverBudgetStateChanged(const nx::vms::api::NetworkBlockData& networkBlockData);
    void handleIoPortStatesChanged(const QnIOStateDataList& portStates);
    void handleFanStateChange(FanState fanState);
    void handleResourcePoolChanges();

private:
    QnMediaServerResourcePtr m_currentServer;

    INetworkBlockManager* m_networkBlockManager = nullptr;
    IIoManager* m_ioManager = nullptr;
    IBuzzerManager* m_buzzerManager = nullptr;
    IFanManager* m_fanManager = nullptr;
    ILedManager* m_ledManager = nullptr;

    QnUuid m_poeOverBudgetHandlerId;
    QnUuid m_alarmOutputHandlerId;
    QnUuid m_fanAlarmHandlerId;

    std::map<QString, bool> m_alarmOutputStates;
};

} // namespace nx::vms::server::nvr::hanwha
