// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_settings_saas_state_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/desktop/resource_properties/server/flux/server_settings_dialog_store.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// ServerSettingsSaasStateWatcher::Private declaration

struct ServerSettingsSaasStateWatcher::Private
{
    ServerSettingsSaasStateWatcher* const q;
    QPointer<ServerSettingsDialogStore> store;
    QnMediaServerResourcePtr server;
    QPointer<nx::vms::common::saas::ServiceManager> serviceManager;

    void updateSaasState();
};

// ------------------------------------------------------------------------------------------------
// ServerSettingsSaasStateWatcher::Private declaration

void ServerSettingsSaasStateWatcher::Private::updateSaasState()
{
    if (!store)
        return;

    store->setSaasState(serviceManager->saasState());
    store->setSaasCloudStorageServicesStatus(
        serviceManager->serviceStatus(nx::vms::api::SaasService::kCloudRecordingType));
}

// ------------------------------------------------------------------------------------------------
// ServerSettingsSaasStateWatcher definition

ServerSettingsSaasStateWatcher::ServerSettingsSaasStateWatcher(
    ServerSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private({this, store}))
{
}

ServerSettingsSaasStateWatcher::~ServerSettingsSaasStateWatcher()
{
}

void ServerSettingsSaasStateWatcher::setServer(const QnMediaServerResourcePtr& server)
{
    if (d->server == server)
        return;

    d->server = server;

    if (!d->server)
    {
        if (d->serviceManager)
            d->serviceManager->disconnect(this);

        d->serviceManager.clear();
    }
    else if (!d->serviceManager)
    {
        d->serviceManager = SystemContext::fromResource(d->server)->saasServiceManager();

        connect(d->serviceManager, &nx::vms::common::saas::ServiceManager::dataChanged, this,
            [this] { d->updateSaasState(); });

        d->updateSaasState();
    }
}

} // nx::vms::client::desktop
