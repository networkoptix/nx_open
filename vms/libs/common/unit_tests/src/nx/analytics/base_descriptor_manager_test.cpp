#include "base_descriptor_manager_test.h"

#include <nx/core/access/access_types.h>
#include <nx/core/resource/server_mock.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

namespace nx::analytics {

namespace {

static const int kServerCount = 10;

} // namespace

void BaseDescriptorManagerTest::SetUp()
{
    makeCommonModule();
    makeServers();
}

void BaseDescriptorManagerTest::makeCommonModule()
{
    m_commonModule = std::make_unique<QnCommonModule>(
        /*clientMode*/ false,
        nx::core::access::Mode::direct);

    m_commonModule->setModuleGUID(QnUuid::createUuid());
}

void BaseDescriptorManagerTest::makeServers()
{
    m_servers.clear();
    for (auto i = 0; i < kServerCount; ++i)
    {
        QnMediaServerResourcePtr server(
            new nx::core::resource::ServerMock(m_commonModule.get()));

        const bool isOwnServer = i == 0;
        server->setId(isOwnServer ? m_commonModule->moduleGUID() : QnUuid::createUuid());
        m_commonModule->resourcePool()->addResource(server);
        m_servers.push_back(server);
    }

    m_servers[0]->setId(m_commonModule->moduleGUID());
}

} // namespace nx::analytics
