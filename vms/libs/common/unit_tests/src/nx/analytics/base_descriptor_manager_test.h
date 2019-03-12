#pragma once

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class BaseDescriptorManagerTest: public ::testing::Test
{
protected:
    virtual void SetUp() override;

    void makeCommonModule();

    void makeServers();

protected:
    std::unique_ptr<QnCommonModule> m_commonModule;
    QnMediaServerResourceList m_servers;

};

} // namespace nx::analytics
