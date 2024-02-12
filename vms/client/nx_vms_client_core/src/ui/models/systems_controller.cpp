// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systems_controller.h"

#include <finders/systems_finder.h>
#include <nx/vms/client/core/settings/systems_visibility_manager.h>
#include <nx/vms/client/core/application_context.h>

using namespace nx::vms::client::core;

SystemsController::SystemsController(QObject* parent): AbstractSystemsController(parent)
{
    NX_ASSERT(qnSystemsFinder, "SystemsFinder is not initialized.");
    NX_ASSERT(qnCloudStatusWatcher, "CloudStatusWatcher is not initialized.");
    NX_ASSERT(qnSystemsVisibilityManager, "SystemsVisibilityManger is not initialized.");

    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemDiscovered,
        this, &AbstractSystemsController::systemDiscovered);

    connect(qnSystemsFinder, &QnAbstractSystemsFinder::systemLost,
        this, &AbstractSystemsController::systemLost);

    connect(qnCloudStatusWatcher, &CloudStatusWatcher::statusChanged,
        this, &AbstractSystemsController::cloudStatusChanged);

    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudLoginChanged,
        this, &AbstractSystemsController::cloudLoginChanged);
}

SystemsController::~SystemsController()
{}

CloudStatusWatcher::Status SystemsController::cloudStatus() const
{
    return qnCloudStatusWatcher->status();
}

QString SystemsController::cloudLogin() const
{
    return qnCloudStatusWatcher->cloudLogin();
}

QnAbstractSystemsFinder::SystemDescriptionList SystemsController::systemsList() const
{
    return qnSystemsFinder->systems();
}

nx::vms::client::core::welcome_screen::TileVisibilityScope SystemsController::visibilityScope(
    const nx::Uuid& localId) const
{
    return qnSystemsVisibilityManager->scope(localId);
}

bool SystemsController::setScopeInfo(
    const nx::Uuid& localId,
    const QString& name,
    nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope)
{
    return qnSystemsVisibilityManager->setScopeInfo(localId, name, visibilityScope);
}
