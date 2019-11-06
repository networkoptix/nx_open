#pragma once

#include <set>

#include <utils/common/connective.h>
#include <api/model/api_ioport_data.h>
#include <core/resource/resource.h>

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/api/data/network_block_data.h>
#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/nvr/i_startable.h>

namespace nx::vms::server::nvr::hanwha {

class LedManager: public Connective<QObject>, public ServerModuleAware, public IStartable
{
public:
    LedManager(QnMediaServerModule* serverModule); //< TODO: #dmishin pass only necessary services.

    virtual ~LedManager();

    virtual void start() override;

private:
    void at_resourceStatusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);

    void handleNetworkBlockState(const nx::vms::api::NetworkBlockData& state);
    void handleIoState(const QnIOStateDataList& state);
    void storeOutputPortIds(const QnIOPortDataList& portDescriptors);
    bool isOutputPort(const QString& portId) const;
    ILedController::LedState calculateAlarmOutputLedState(const QnIOStateDataList& state) const;

private:
    ILedController* m_ledController = nullptr;
    INetworkBlockController* m_networkBlockController = nullptr;
    IIoController* m_ioController = nullptr;

    QnUuid m_networkBlockControllerHandlerId;
    QnUuid m_ioControllerHandlerId;
    std::set<QString> m_outputPortIds;
};

} // namespace nx::vms::server::nvr::hanwha
