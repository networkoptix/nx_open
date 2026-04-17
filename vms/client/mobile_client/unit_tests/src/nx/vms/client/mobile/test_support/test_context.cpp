// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_context.h"

#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_startup_parameters.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/mobile/application_context.h>

namespace nx::vms::client::mobile::test {

namespace {

static std::unique_ptr<ApplicationContext> sAppContext;

} // namespace

Context::Context()
{
    NX_ASSERT(appContext());
}

SystemContext* Context::systemContext() const
{
    return appContext()->currentSystemContext();
}

QnLayoutResourcePtr ApplicationContextBasedTest::createLayout() const
{
    core::LayoutResourcePtr layout(new core::LayoutResource());
    layout->setIdUnsafe(nx::Uuid::createUuid());
    return layout;
}

void ApplicationContextBasedTest::SetUpTestSuite()
{
    QnMobileClientStartupParameters startupParams;
    std::unique_ptr<QnMobileClientSettings> settings(new QnMobileClientSettings());

    sAppContext = std::make_unique<ApplicationContext>(
        startupParams,
        std::move(settings),
        core::ApplicationContext::Mode::unitTests);
}

void ApplicationContextBasedTest::TearDownTestSuite()
{
    sAppContext.reset();
}

} // namespace nx::vms::client::mobile::test
