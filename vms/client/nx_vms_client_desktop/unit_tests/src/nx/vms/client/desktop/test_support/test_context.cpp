// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_context.h"

#include <QtCore/QCoreApplication>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/message_processor_mock.h>

namespace nx::vms::client::desktop::test {

struct Context::Private
{
    std::unique_ptr<ApplicationContext> appContext;
};

Context::Context():
    d(new Private())
{
    QCoreApplication::setOrganizationName(nx::branding::company());
    QCoreApplication::setApplicationName("Unit tests");
    d->appContext = std::make_unique<ApplicationContext>(
        ApplicationContext::Mode::unitTests,
        QnStartupParameters());
}

Context::~Context()
{
    if (systemContext()->messageProcessor())
        systemContext()->deleteMessageProcessor();

    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setApplicationName(QString());
}

QnCommonModule* Context::commonModule() const
{
    return d->appContext->clientCoreModule()->commonModule();
}

QnClientCoreModule* Context::clientCoreModule() const
{
    return d->appContext->clientCoreModule();
}

SystemContext* Context::systemContext() const
{
    return d->appContext->currentSystemContext();
}

MessageProcessorMock* ContextBasedTest::createMessageProcessor()
{
    return systemContext()->createMessageProcessor<MessageProcessorMock>();
}

} // namespace nx::vms::client::desktop::test
