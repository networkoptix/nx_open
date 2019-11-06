#pragma once

#include <utils/common/connective.h>
#include <api/model/api_ioport_data.h>
#include <core/resource/resource.h>

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/api/data/network_block_data.h>

namespace nx::vms::server::nvr::hanwha {

class LedManager: public Connective<QObject>, public ServerModuleAware
{
public:
    LedManager(QnMediaServerModule* serverModule);

    virtual ~LedManager();

    void at_resourceStatusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);

private:
    void handleNetworkBlockState(const nx::vms::api::NetworkBlockData& state);
    void handleIoState(const QnIOStateDataList& state);

private:
    QnUuid m_networkBlockControllerHandlerId;
    QnUuid m_ioControllerHandlerId;
};

} // namespace nx::vms::server::nvr::hanwha
