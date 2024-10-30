// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_specific_settings.h"

#include <chrono>

#include <core/resource/user_resource.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/settings/user_property_backend.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/connect_actions_handler.h>
#include <nx/vms/client/desktop/window_context.h>

using namespace std::chrono;

namespace {

const QString kResourcePropertyName = "desktopClientSettings";

} // namespace

namespace nx::vms::client::desktop {

UserSpecificSettings::UserSpecificSettings(SystemContext* systemContext):
    Storage(new core::property_storage::UserPropertyBackend(
        systemContext,
        kResourcePropertyName,
        [systemContext]() { return systemContext->restApiHelper()->getSessionTokenHelper(); })),
    SystemContextAware(systemContext),
    m_pendingSync(new nx::utils::PendingOperation(this))
{
    m_pendingSync->setCallback([this]() { sync(); });
    m_pendingSync->setInterval(10s);

    connect(systemContext, &SystemContext::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            sync();

            if (user)
            {
                load();
                emit loaded();
            }
        });

    connect(this, &Storage::changed, this,
        [this]()
        {
            const ConnectActionsHandler::LogicalState state =
                appContext()->mainWindowContext()->connectActionsHandler()->logicalState();

            if (state == ConnectActionsHandler::LogicalState::connected)
                m_pendingSync->requestOperation();
        });
}

UserSpecificSettings::~UserSpecificSettings()
{
}

void UserSpecificSettings::saveImmediately()
{
    m_pendingSync->cancel();
    sync();
}

} // namespace nx::vms::client::desktop
