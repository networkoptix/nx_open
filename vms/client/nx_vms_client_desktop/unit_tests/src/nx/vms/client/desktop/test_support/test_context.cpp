// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_context.h"

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop::test {

struct Context::Private
{
    std::unique_ptr<QnStaticCommonModule> staticCommonModule;
    std::unique_ptr<QnClientRuntimeSettings> clientRuntimeSettings;
    std::unique_ptr<SystemContext> systemContext;
    std::unique_ptr<QnClientCoreModule> clientCoreModule;
};

Context::Context():
    d(new Private())
{
    d->staticCommonModule = std::make_unique<QnStaticCommonModule>();
    d->clientRuntimeSettings = std::make_unique<QnClientRuntimeSettings>(QnStartupParameters());
    d->systemContext = std::make_unique<SystemContext>(/*peerId*/ QnUuid::createUuid());
    d->clientCoreModule = std::make_unique<QnClientCoreModule>(
        QnClientCoreModule::Mode::unitTests,
        d->systemContext.get());
}

Context::~Context()
{
}

QnStaticCommonModule* Context::staticCommonModule() const
{
    return d->staticCommonModule.get();
}

QnCommonModule* Context::commonModule() const
{
    return d->clientCoreModule->commonModule();
}

QnClientCoreModule* Context::clientCoreModule() const
{
    return d->clientCoreModule.get();
}

SystemContext* Context::systemContext() const
{
    return d->systemContext.get();
}

} // namespace nx::vms::client::desktop::test
