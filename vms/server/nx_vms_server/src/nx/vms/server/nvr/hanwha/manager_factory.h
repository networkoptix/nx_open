#pragma once

#include <map>

#include <core/resource/resource_fwd.h>

#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/server/nvr/i_manager_factory.h>

class QnResourcePool;

namespace nx::vms::server {

class RootFileSystem;
class IPoweringPolicyFactory;

} // namespace nx::vms::server

namespace nx::vms::server::nvr::hanwha {

class ManagerFactory: public IManagerFactory, ServerModuleAware
{
public:
    ManagerFactory(
        QnMediaServerModule* serverModule,
        DeviceInfo deviceInfo);

    virtual ~ManagerFactory() override;

    virtual std::unique_ptr<INetworkBlockManager> createNetworkBlockManager() const override;

    virtual std::unique_ptr<IIoManager> createIoManager() const override;

    virtual std::unique_ptr<IBuzzerManager> createBuzzerManager() const override;

    virtual std::unique_ptr<IFanManager> createFanManager() const override;

    virtual std::unique_ptr<ILedManager> createLedManager() const override;

private:
    int getDescriptorByDeviceFileName(const QString& deviceFileName) const;

private:
    DeviceInfo m_deviceInfo;

    mutable std::map<QString, int> m_descriptorByDeviceFileName;
};

} // namespace nx::vms::server::nvr::hanwha
