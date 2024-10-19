// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <mobile_client/mobile_client_message_processor.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::mobile {

struct ApplicationContext::Private
{
    std::unique_ptr<SystemContext> systemContext;
};

ApplicationContext::ApplicationContext(QObject* parent):
    base_type(Mode::mobileClient,
        nx::vms::api::PeerType::mobileClient,
        qnSettings->customCloudHost(),
        qnSettings->ignoreCustomization(),
        /*customExternalResourceFile*/ {},
        parent),
    d(new Private{})
{
    initializeNetworkModules();
    const auto selectedLocale = coreSettings()->locale();
    initializeTranslations(selectedLocale.isEmpty()
        ? QLocale::system().name()
        : selectedLocale);

    d->systemContext = std::make_unique<SystemContext>(
        core::SystemContext::Mode::client,
        /*peerId*/ nx::Uuid::createUuid());
    addSystemContext(d->systemContext.get());
    d->systemContext->createMessageProcessor<QnMobileClientMessageProcessor>(
        d->systemContext.get());
}

ApplicationContext::~ApplicationContext()
{
    resetEngine(); // Frees all systems contexts of the QML objects.

    // DesktopCameraConnection is a SystemContextAware object, so it should be deleted before the
    // SystemContext.
    common::ApplicationContext::stopAll();

    d->systemContext->setSession({});
    if (d->systemContext->messageProcessor())
        d->systemContext->deleteMessageProcessor();
    d->systemContext.reset();
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    return base_type::currentSystemContext()->as<SystemContext>();
}

} // namespace nx::vms::client::mobile
