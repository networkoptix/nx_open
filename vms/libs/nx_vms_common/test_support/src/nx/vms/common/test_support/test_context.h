// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/core/access/access_types.h>
#include <nx/utils/impl_ptr.h>

#include "resource/resource_pool_test_helper.h"

class QnCommonModule;

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::common::test {

class MessageProcessorMock;

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

class NX_VMS_COMMON_TEST_SUPPORT_API ContextBasedTestBase:
    public ::testing::Test
{
public:
    ContextBasedTestBase(nx::core::access::Mode resourceAccessMode);
    ~ContextBasedTestBase();

    Context* context() const { return m_context.get(); }

    MessageProcessorMock* createMessageProcessor();

private:
    std::unique_ptr<Context> m_context;
};

// QnResourcePoolTestHelper must be initialized after context already created.
class NX_VMS_COMMON_TEST_SUPPORT_API ContextBasedTest:
    public ContextBasedTestBase,
    protected QnResourcePoolTestHelper
{
public:
    ContextBasedTest(nx::core::access::Mode resourceAccessMode = nx::core::access::Mode::direct);
};

} // namespace nx::vms::common::test
