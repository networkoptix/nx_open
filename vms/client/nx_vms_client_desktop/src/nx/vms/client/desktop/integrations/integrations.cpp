// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integrations.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/ini.h>

#include "config.h"
#include "interfaces.h"
#include "nvr/overlapped_id.h"

namespace nx::vms::client::desktop::integrations {

struct Storage::Private
{
    // Keeping raw pointers as all integrations are owned by this class.
    QList<Integration*> allIntegrations;
    QList<IServerApiProcessor*> serverApiProcessors;
    QList<IMenuActionFactory*> actionFactories;
    QList<IOverlayPainter*> overlayPainters;

    void addIntegration(Integration* integration)
    {
        allIntegrations.push_back(integration);
        if (const auto serverApiProcessor = dynamic_cast<IServerApiProcessor*>(integration))
            serverApiProcessors.push_back(serverApiProcessor);
        if (const auto actionFactory = dynamic_cast<IMenuActionFactory*>(integration))
            actionFactories.push_back(actionFactory);
        if (const auto overlayPainter = dynamic_cast<IOverlayPainter*>(integration))
            overlayPainters.push_back(overlayPainter);
    }
};

Storage::Storage(bool desktopMode): d(new Private())
{
    if (desktopMode)
        d->addIntegration(new OverlappedIdIntegration(this));
}

Storage::~Storage()
{
}

void Storage::connectionEstablished(
    const QnUserResourcePtr& currentUser,
    ec2::AbstractECConnectionPtr connection)
{
    for (const auto serverApiProcessor: d->serverApiProcessors)
        serverApiProcessor->connectionEstablished(currentUser, connection);
}

void Storage::registerActions(menu::MenuFactory* factory)
{
    for (const auto actionFactory: d->actionFactories)
        actionFactory->registerActions(factory);
}

void Storage::registerWidget(QnMediaResourceWidget* widget)
{
    for (const auto overlayPainter: d->overlayPainters)
        overlayPainter->registerWidget(widget);
}

void Storage::unregisterWidget(QnMediaResourceWidget* widget)
{
    for (const auto overlayPainter: d->overlayPainters)
        overlayPainter->unregisterWidget(widget);
}

void Storage::paintVideoOverlays(QnMediaResourceWidget* widget, QPainter* painter)
{
    for (const auto overlayPainter: d->overlayPainters)
        overlayPainter->paintVideoOverlay(widget, painter);
}

} // namespace nx::vms::client::desktop::integrations
