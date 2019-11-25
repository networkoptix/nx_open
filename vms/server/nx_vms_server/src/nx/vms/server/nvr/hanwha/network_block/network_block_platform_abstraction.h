#pragma once

#include <nx/vms/server/nvr/hanwha/network_block/i_network_block_platform_abstraction.h>

namespace nx::vms::server { class RootFileSystem; }

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockPlatformAbstractionImpl;

class NetworkBlockPlatformAbstraction: public INetworkBlockPlatformAbstraction
{
public:
    NetworkBlockPlatformAbstraction(RootFileSystem* rootFileSystem);

    virtual ~NetworkBlockPlatformAbstraction();

    virtual int portCount() const override;
    virtual NetworkPortState portState(int portNumber) const override;

    virtual bool setPoeEnabled(int portNumber, bool isPoeEnabled) override;

    virtual void interrupt() override;

private:
    std::unique_ptr<NetworkBlockPlatformAbstractionImpl> m_impl;
};

} // namespace nx::vms::server::nvr::hanwha
