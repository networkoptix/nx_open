#include "service.h"

#include <nx/vms/server/nvr/hanwha/io_manager.h>
#include <nx/vms/server/nvr/hanwha/led_manager.h>
#include <nx/vms/server/nvr/hanwha/network_block_manager.h>
#include <nx/vms/server/nvr/hanwha/buzzer_manager.h>

namespace nx::vms::server::nvr::hanwha {

Service::Service():
    m_buzzerManager(std::make_unique<BuzzerManager>()),
    m_networkBlockManager(std::make_unique<NetworkBlockManager>()),
    m_ioManager(std::make_unique<IoManager>()),
    m_ledManager(std::make_unique<LedManager>())
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

} // namespace nx::vms::server::nvr::hanwha
