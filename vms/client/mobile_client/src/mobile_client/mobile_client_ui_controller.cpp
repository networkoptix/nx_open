// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_ui_controller.h"

#include <core/resource/resource.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/session/ui_messages.h>
#include <nx/vms/client/mobile/ui/cloud_action_handler.h>
#include <nx/vms/client/mobile/utils/operation_manager.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/delayed.h>

namespace {

std::pair<QString, QString> connectionErrorTitleAndText(
    nx::vms::client::core::RemoteConnectionErrorCode errorCode,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    const auto title = moduleInformation.systemName.isEmpty()
        ? QnMobileClientUiController::tr("Cannot connect to the Server")
        : QnMobileClientUiController::tr("Cannot connect to the Site \"%1\"", "%1 is a site name")
              .arg(moduleInformation.systemName);

    return {title,
        nx::vms::client::mobile::UiMessages::getConnectionErrorText(errorCode, moduleInformation)};
}

} // namespace

struct QnMobileClientUiController::Private
{
    QnMobileClientUiController* const q;
    std::unique_ptr<nx::vms::client::mobile::OperationManager> operationManager;
    QnResourcePtr resource;
    bool avoidHandlingConnectionStuff = false;
    QnMobileClientUiController::Screen currentScreen =
        QnMobileClientUiController::Screen::UnknownScreen;

    void handleSessionError(nx::vms::client::core::RemoteConnectionErrorCode errorCode,
        const nx::vms::api::ModuleInformation& moduleInformation);
    void showSessionError(nx::vms::client::core::RemoteConnectionErrorCode errorCode,
        const nx::vms::api::ModuleInformation& moduleInformation);
};

void QnMobileClientUiController::Private::handleSessionError(
    nx::vms::client::core::RemoteConnectionErrorCode errorCode,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    NX_DEBUG(q, "handleSessionError(): current screen is <%1>", q->currentScreen());

    if (q->avoidHandlingConnectionStuff())
        return;

    nx::vms::client::core::appContext()->coreSettings()->lastConnection = {};

    if (errorCode != nx::vms::client::core::RemoteConnectionErrorCode::truncatedSessionToken)
    {
        showSessionError(errorCode, moduleInformation);
        return;
    }

    q->windowContext()->cloudActionHandler()->requestCloudReauthorization(q,
        [this, errorCode, moduleInformation](
            std::optional<nx::vms::client::core::OauthDataRequestError> error)
        {
            // The connection state may change while the screen is open.
            if (q->avoidHandlingConnectionStuff() || q->sessionManager()->hasSession())
            {
                NX_DEBUG(q, "handleSessionError(): connection state changed, result is ignored");
                return;
            }

            if (error)
            {
                showSessionError(errorCode, moduleInformation);
                return;
            }

            // The new cloud auth data is already applied on success.
            q->sessionManager()->startCloudSession(
                moduleInformation.cloudSystemId, moduleInformation.systemName);
        });
}

void QnMobileClientUiController::Private::showSessionError(
    nx::vms::client::core::RemoteConnectionErrorCode errorCode,
    const nx::vms::api::ModuleInformation& moduleInformation)
{
    using nx::vms::client::core::RemoteConnectionErrorCode;
    const auto [title, errorText] = connectionErrorTitleAndText(errorCode, moduleInformation);

    const bool is2faError = errorCode == RemoteConnectionErrorCode::systemIsNotCompatibleWith2Fa
        || errorCode == RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled;

    if (is2faError)
        emit q->twoFactorAuthError(title, errorText);
    else
        emit q->genericError(title, errorText);
}

//-------------------------------------------------------------------------------------------------

QnMobileClientUiController::QnMobileClientUiController(
    nx::vms::client::mobile::WindowContext* context, QObject* parent):
    base_type(parent),
    nx::vms::client::mobile::WindowContextAware(context),
    d(new Private{.q = this,
        .operationManager = std::make_unique<nx::vms::client::mobile::OperationManager>()})
{
    NX_DEBUG(this, "QnMobileClientUiController(): created mobile client UI controller");

    connect(sessionManager(), &SessionManager::hasSessionChanged, this,
        [this]()
        {
            const auto screen = currentScreen();

            NX_DEBUG(this, "initialize(): hasSessionChanged: screen is <%1>, has session is <%2>",
                screen, sessionManager()->hasSession());

            if (avoidHandlingConnectionStuff())
                return;

            if (sessionManager()->hasSession())
            {
                openResourcesScreen();
            }
            else
            {
                setRawResource({});
                if (currentScreen() != Screen::DigestLoginToCloudScreen)
                    openSessionsScreen();
            }
        });

    connect(sessionManager(), &SessionManager::sessionStoppedManually, this,
        [this]()
        {
            NX_DEBUG(this, "initialize(): sessionStoppedManually(): called");
            nx::vms::client::core::appContext()->coreSettings()->lastConnection = {};
        });

    connect(sessionManager(), &SessionManager::sessionStartedSuccessfully, this,
        [this]()
        {
            NX_DEBUG(this, "initialize(): sessionStartedSuccessfully(): current screen is <%1>",
                currentScreen());
            if (currentScreen() != Screen::ResourcesScreen)
                emit resourcesScreenRequested(UuidList{});
        });

    connect(sessionManager(),
        &SessionManager::sessionFinishedWithError,
        this,
        [this](nx::vms::client::core::RemoteConnectionErrorCode errorCode,
            const nx::vms::api::ModuleInformation& moduleInformation)
        { d->handleSessionError(errorCode, moduleInformation); });
}

QnMobileClientUiController::~QnMobileClientUiController()
{
}

QnMobileClientUiController::Screen QnMobileClientUiController::currentScreen() const
{
    return d->currentScreen;
}

void QnMobileClientUiController::setCurrentScreen(Screen value)
{
    if (d->currentScreen == value)
        return;

    d->currentScreen = value;
    emit currentScreenChanged();
}

QnResource* QnMobileClientUiController::rawResource() const
{
    return d->resource.get();
}

void QnMobileClientUiController::setRawResource(QnResource* value)
{
    if (d->resource == value)
        return;

    d->resource = value ? value->toSharedPointer() : QnResourcePtr{};
    emit resourceChanged();
}

bool QnMobileClientUiController::avoidHandlingConnectionStuff() const
{
    return d->avoidHandlingConnectionStuff;
}

void QnMobileClientUiController::setAvoidHandlingConnectionStuff(bool value)
{
    if (d->avoidHandlingConnectionStuff == value)
        return;

    d->avoidHandlingConnectionStuff = value;
    emit avoidHandlingConnectionStuffChanged();
}

nx::vms::client::mobile::OperationManager* QnMobileClientUiController::operationManager() const
{
    return d->operationManager.get();
}

void QnMobileClientUiController::openConnectToServerScreen(
    const nx::Url& url,
    const QString& operationId)
{
    NX_DEBUG(this, "openConnectToServerScreen(): url is <%1>", url.toString(QUrl::RemovePassword));
    emit connectToServerScreenRequested(
        url.displayAddress(), url.userName(), url.password(), operationId);
}

void QnMobileClientUiController::openResourcesScreen(const UuidList& filterIds)
{
    NX_DEBUG(this, "openResourcesScreen(): resources count is <%1>", filterIds.size());
    emit resourcesScreenRequested(filterIds);
}

void QnMobileClientUiController::openVideoScreen(QnResource* cameraResource, qint64 timestamp)
{
    NX_DEBUG(this, "openVideoScreen(): camera is <%1>, timestamp is <%2>",
        (cameraResource ? cameraResource->getId() : nx::Uuid{}), timestamp);
    emit videoScreenRequested(cameraResource, timestamp);
}

void QnMobileClientUiController::showTwoFactorAuthError(const QString& systemName)
{
    nx::vms::api::ModuleInformation moduleInformation;
    moduleInformation.systemName = systemName;

    const auto [title, errorText] = connectionErrorTitleAndText(
        nx::vms::client::core::RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled,
        moduleInformation);
    emit twoFactorAuthError(title, errorText);
}

void QnMobileClientUiController::openSessionsScreen()
{
    NX_DEBUG(this, "openSessionsScreen(): called");
    emit sessionsScreenRequested();
}
