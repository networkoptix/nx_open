// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_context.h"

#include <QtCore/QCoreApplication>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/message_processor_mock.h>

namespace nx::vms::client::desktop::test {

namespace {

static std::unique_ptr<ApplicationContext> sAppContext;

} // namespace

Context::Context()
{
    NX_ASSERT(appContext(), "Desktop client System Context heavily depends on Application Context "
        "existance. Use AppContextBasedTest as a base class instead.");
}

Context::~Context()
{
}

SystemContext* Context::systemContext() const
{
    return appContext()->currentSystemContext();
}

MessageProcessorMock* SystemContextBasedTest::createMessageProcessor()
{
    auto messageProcessor = systemContext()->createMessageProcessor<MessageProcessorMock>();

    systemContext()->otherServersManager()->setMessageProcessor(messageProcessor);

    return messageProcessor;
}

QnLayoutResourcePtr SystemContextBasedTest::createLayout()
{
    LayoutResourcePtr layout(new LayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    return layout;
}

void SystemContextBasedTest::initAppContext(ApplicationContext::Features features)
{
    sAppContext = std::make_unique<ApplicationContext>(
        ApplicationContext::Mode::unitTests,
        QnStartupParameters(),
        features);
}

void SystemContextBasedTest::deinitAppContext()
{
    sAppContext.reset();
}

void SystemContextBasedTest::TearDown()
{
    base_type::TearDown();
    if (systemContext()->messageProcessor())
        systemContext()->deleteMessageProcessor();
}

} // namespace nx::vms::client::desktop::test
