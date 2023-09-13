// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/core/access/access_types.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/system_context.h>

#include "resource/resource_pool_test_helper.h"

class QnCommonModule;

namespace nx::vms::common::test {

class MessageProcessorMock;

template<typename ContextType, typename... ContextCreationArgs>
class ContextHolder
{
public:
    ContextHolder(ContextCreationArgs... args): m_context(std::make_unique<ContextType>(args...)) {}
    ContextType* context() const { return m_context.get(); }

private:
    const std::unique_ptr<ContextType> m_context;
};

template<typename ContextType, typename...ContextCreationArgs>
class GenericContextBasedTest:
    public ::testing::Test,
    public ContextHolder<ContextType, ContextCreationArgs...>,
    protected QnResourcePoolTestHelper
{
public:
    explicit GenericContextBasedTest(ContextCreationArgs... args):
        ContextHolder<ContextType, ContextCreationArgs...>(args...),
        QnResourcePoolTestHelper(this->context()->systemContext())
    {
    }

    auto systemContext() const { return this->context()->systemContext(); }
    virtual void TearDown() override { clear(); }
};

class NX_VMS_COMMON_TEST_SUPPORT_API Context
{
public:
    Context(nx::core::access::Mode resourceAccessMode = nx::core::access::Mode::direct);
    virtual ~Context();

    QnCommonModule* commonModule() const;
    SystemContext* systemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class NX_VMS_COMMON_TEST_SUPPORT_API ContextBasedTest:
    public GenericContextBasedTest<Context, nx::core::access::Mode>
{
public:
    ContextBasedTest(nx::core::access::Mode resourceAccessMode = nx::core::access::Mode::direct);
    virtual ~ContextBasedTest() override;

    MessageProcessorMock* createMessageProcessor();
};

} // namespace nx::vms::common::test
