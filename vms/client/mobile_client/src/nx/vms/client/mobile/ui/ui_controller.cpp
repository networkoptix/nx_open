// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ui_controller.h"

#include <mobile_client/mobile_client_settings.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>
#include <nx/vms/client/mobile/window_context.h>

#include "detail/measurements.h"
#include "detail/screens.h"
#include "detail/window_helpers.h"

namespace nx::vms::client::mobile {

namespace {

QString titleForConnectionError(const QString& systemName)
{
    return systemName.isEmpty()
        ? UiController::tr("Cannot connect to the Server")
        : UiController::tr("Cannot connect to the Site \"%1\"", "%1 is a site name").arg(systemName);
}

} // namespace

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

void UiController::showConnectionErrorMessage(
    core::RemoteConnectionErrorCode errorCode,
    const QString& systemName,
    const QString& errorText) const
{
    const bool is2faError =
        errorCode == core::RemoteConnectionErrorCode::systemIsNotCompatibleWith2Fa ||
        errorCode == core::RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled;
    if (!is2faError)
    {
        showConnectionErrorMessage(systemName, errorText);
        return;
    }

    QmlWrapperHelper::showPopup(
        windowContext(),
        QUrl("Nx/Web/TwoFactorAuthenticationErrorDialog.qml"),
        QVariantMap {
            {"title", titleForConnectionError(systemName)},
            {"messages", QStringList{errorText}}
        });
}

bool UiController::tryRestoreLastUsedConnection()
{
    // TODO: 5.1+ Make startup parameters handling and using of last(used)Connection the same both
    // in the desktop and mobile clients.

    const auto parameters = qnSettings->startupParameters();
    const auto connectionUrl = parameters.url;
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
    QmlWrapperHelper::showPopup(windowContext(), QUrl("Nx/Mobile/Popups/StandardPopup.qml"),
        QVariantMap {
            {"title", title},
            {"messages", QStringList{message}}
        });
}

void UiController::showConnectionErrorMessage(
    const QString& systemName,
    const QString& errorText) const
{
    showMessage(titleForConnectionError(systemName), errorText);
}

void UiController::showLinkAboutToOpenMessage(const QString& link) const
{
    QmlWrapperHelper::showPopup(
        windowContext(),
        QUrl("Nx/Web/LinkAboutToOpenDialog.qml"),
        QVariantMap {{"link", link}});
}

} // namespace nx::vms::client::mobile
