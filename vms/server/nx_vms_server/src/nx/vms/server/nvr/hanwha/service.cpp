#include "service.h"

#include <nx/vms/server/nvr/hanwha/io_manager.h>
#include <nx/vms/server/nvr/hanwha/led_manager.h>
#include <nx/vms/server/nvr/hanwha/network_block_manager.h>
#include <nx/vms/server/nvr/hanwha/buzzer_manager.h>
#include <nx/vms/server/nvr/hanwha/io_module_searcher.h>

namespace nx::vms::server::nvr::hanwha {

Service::Service(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule),
    m_buzzerManager(std::make_unique<BuzzerManager>()),
    m_networkBlockManager(std::make_unique<NetworkBlockManager>()),
    m_ioManager(std::make_unique<IoManager>()),
    m_ledManager(std::make_unique<LedManager>()),
    m_searcher(std::make_unique<IoModuleSearcher>(serverModule))
{
}

IBuzzerManager* Service::buzzerManager()
{
    return m_buzzerManager.get();
}

INetworkBlockManager* Service::networkBlockManager()
{
    return m_networkBlockManager.get();
}

IIoManager* Service::ioManager()
{
    return m_ioManager.get();
}

ILedManager* Service::ledManager()
{
    return m_ledManager.get();
}

QnAbstractResourceSearcher* Service::searcher()
{
    return m_searcher.get();
}

} // namespace nx::vms::server::nvr::hanwha
