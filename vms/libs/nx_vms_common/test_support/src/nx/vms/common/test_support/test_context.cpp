// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_context.h"

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/vms/common/system_context.h>

#include "api/message_processor_mock.h"

namespace nx::vms::common::test {

struct Context::Private
{
    std::unique_ptr<SystemContext> systemContext;
    std::unique_ptr<QnCommonModule> commonModule;
};

Context::Context(nx::core::access::Mode resourceAccessMode):
    d(new Private())
{
    d->systemContext = std::make_unique<SystemContext>(
        SystemContext::Mode::unitTests,
        /*peerId*/ QnUuid::createUuid(),
        /*sessionId*/ QnUuid::createUuid(),
        resourceAccessMode);
    d->commonModule = std::make_unique<QnCommonModule>(d->systemContext.get());
}

Context::~Context()
{
}

QnCommonModule* Context::commonModule() const
{
    return d->commonModule.get();
}

SystemContext* Context::systemContext() const
{
    return d->systemContext.get();
}

//-------------------------------------------------------------------------------------------------
// ContextBasedTestBase

ContextBasedTestBase::ContextBasedTestBase(nx::core::access::Mode resourceAccessMode):
    m_context(std::make_unique<Context>(resourceAccessMode))
{
}

ContextBasedTestBase::~ContextBasedTestBase()
{
    if (m_context->systemContext()->messageProcessor())
        m_context->systemContext()->deleteMessageProcessor();
}

MessageProcessorMock* ContextBasedTestBase::createMessageProcessor()
{
    return m_context->systemContext()->createMessageProcessor<MessageProcessorMock>();
}

//-------------------------------------------------------------------------------------------------
// ContextBasedTest

ContextBasedTest::ContextBasedTest(nx::core::access::Mode resourceAccessMode):
    ContextBasedTestBase(resourceAccessMode),
    QnResourcePoolTestHelper(this->context()->systemContext())
{
}

} // namespace nx::vms::common::test
