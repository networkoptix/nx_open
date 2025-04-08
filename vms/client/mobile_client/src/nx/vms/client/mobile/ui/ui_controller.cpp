// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_controller.h"

#include <mobile_client/mobile_client_settings.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>

#include "detail/measurements.h"
#include "detail/screens.h"
#include "detail/window_helpers.h"

namespace nx::vms::client::mobile {

struct UiController::Private
{
    UiController* const q;
    std::unique_ptr<detail::Measurements> measurements;
    std::unique_ptr<detail::WindowHelpers> windowHelpers;
    std::unique_ptr<detail::Screens> screens;

    void initializeMarginsStuff();
};

//-------------------------------------------------------------------------------------------------

void UiController::registerQmlType()
{
//    qmlRegisterType<UiController>("nx.vms.client.mobile", 1, 0, "MobileUiController");
}

UiController::UiController(WindowContext* context,
    QObject* parent)
    :
    QObject(parent),
    base_type(context),
    d{ new Private{
        .q = this,
        .measurements = std::make_unique<detail::Measurements>(context->window()),
        .windowHelpers = std::make_unique<detail::WindowHelpers>(context),
        .screens = std::make_unique<detail::Screens>(context)}}
{
}

UiController::~UiController()
{
}

detail::Measurements* UiController::measurements() const
{
    return d->measurements.get();
}

detail::WindowHelpers* UiController::windowHelpers() const
{
    return d->windowHelpers.get();
}

detail::Screens* UiController::screens() const
{
    return d->screens.get();
}

bool UiController::tryRestoreLastUsedConnection()
{
    // TODO: 5.1+ Make startup parameters handling and using of last(used)Connection the same both
    // in the desktop and mobile clients.

    const auto connectionUrl = qnSettings->startupParameters().url;
    if (!connectionUrl.isEmpty())
    {
        sessionManager()->startSessionByUrl(connectionUrl);
        NX_DEBUG(this, "Connecting to the system by the url specified in startup parameters: %1",
            connectionUrl.toString(QUrl::RemovePassword));
        return true;
    }

    const auto& connectionData = appContext()->coreSettings()->lastConnection();
    const auto& systemId = connectionData.systemId;
    const auto& url = connectionData.url;
    if (!url.isValid() || systemId.isNull())
    {
        NX_DEBUG(this, "No last used session specified.");
        return false;
    }

    if (sessionManager()->startSessionWithStoredCredentials(url, systemId, url.userName()))
    {
        NX_DEBUG(this, "Restoring last used local session.");
        return true;
    }

    if (appContext()->cloudStatusWatcher()->status() != core::CloudStatusWatcher::LoggedOut)
    {
        NX_DEBUG(this, "Restoring last used cloud session.");
        sessionManager()->startCloudSession(systemId.toSimpleString());
        return true;
    }

    NX_DEBUG(this, "Can't restore last used session since user is not logged to the cloud.");
    return false;
}

void UiController::showMessage(const QString& title, const QString& message) const
{
    QmlWrapperHelper::showPopup(windowContext(), QUrl("Nx/Dialogs/StandardDialog.qml"),
        QVariantMap {
            {"title", title},
            {"message", message}
        });
}

void UiController::showConnectionErrorMessage(
    const QString& systemName,
    const QString& errorText)
{
    const auto title = systemName.isEmpty()
        ? tr("Cannot connect to the Server")
        : tr("Cannot connect to the Site \"%1\"", "%1 is a site name").arg(systemName);
    showMessage(title, errorText);
}

} // namespace nx::vms::client::mobile
