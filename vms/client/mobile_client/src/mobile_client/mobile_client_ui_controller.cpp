// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_ui_controller.h"

#include <core/resource/resource.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/session/ui_messages.h>
#include <nx/vms/client/mobile/ui/detail/screens.h>
#include <nx/vms/client/mobile/ui/ui_controller.h>
#include <nx/vms/client/mobile/utils/operation_manager.h>
#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/delayed.h>

class QnMobileClientUiControllerPrivate
{
public:
    std::unique_ptr<nx::vms::client::mobile::OperationManager> operationManager;
    QnResourcePtr resource;
    bool avoidHandlingConnectionStuff = false;
    QnMobileClientUiController::Screen currentScreen =
        QnMobileClientUiController::Screen::UnknownScreen;
};

QnMobileClientUiController::QnMobileClientUiController(
    nx::vms::client::mobile::WindowContext* context,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::client::mobile::WindowContextAware(context),
    d_ptr(new QnMobileClientUiControllerPrivate{
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
                emit resourcesScreenRequested(QVariant());
        });

    using RemoteConnectionErrorCode = nx::vms::client::core::RemoteConnectionErrorCode;
    const auto handleSessionError =
        [this](
            RemoteConnectionErrorCode errorCode,
            const nx::vms::api::ModuleInformation& moduleInformation)
        {
            NX_DEBUG(this, "initialize(): sessionFinishedWithError: current screen is <%1>",
                currentScreen());

            if (avoidHandlingConnectionStuff())
                return;

            nx::vms::client::core::appContext()->coreSettings()->lastConnection = {};
            const QString user =
                nx::vms::client::core::appContext()->cloudStatusWatcher()->cloudLogin();

            if (errorCode == RemoteConnectionErrorCode::truncatedSessionToken
                && windowContext()->uiController()->screens()->showCloudLoginScreen(
                    /*reauthentication*/ true, user))
            {
                connect(nx::vms::client::core::appContext()->cloudStatusWatcher(),
                    &nx::vms::client::core::CloudStatusWatcher::credentialsChanged,
                    this,
                    [this, moduleInformation]()
                    {
                        sessionManager()->startCloudSession(
                            moduleInformation.cloudSystemId, moduleInformation.systemName);
                    },
                    Qt::SingleShotConnection);
            }
            else
            {
                using UiMessages = nx::vms::client::mobile::UiMessages;
                const auto errorText =
                    UiMessages::getConnectionErrorText(errorCode, moduleInformation);

                uiController()->showConnectionErrorMessage(
                    errorCode, moduleInformation.systemName, errorText);
            }
        };
    connect(sessionManager(), &SessionManager::sessionFinishedWithError, this, handleSessionError);
}

QnMobileClientUiController::~QnMobileClientUiController()
{
}

QnMobileClientUiController::Screen QnMobileClientUiController::currentScreen() const
{
    Q_D(const QnMobileClientUiController);
    return d->currentScreen;
}

void QnMobileClientUiController::setCurrentScreen(Screen value)
{
    Q_D(QnMobileClientUiController);
    if (d->currentScreen == value)
        return;

    d->currentScreen = value;
    emit currentScreenChanged();
}


QnResource* QnMobileClientUiController::rawResource() const
{
    Q_D(const QnMobileClientUiController);
    return d->resource.get();
}

void QnMobileClientUiController::setRawResource(QnResource* value)
{
    Q_D(QnMobileClientUiController);

    if (d->resource == value)
        return;

    d->resource = value ? value->toSharedPointer() : QnResourcePtr{};
    emit resourceChanged();
}

bool QnMobileClientUiController::avoidHandlingConnectionStuff() const
{
    Q_D(const QnMobileClientUiController);
    return d->avoidHandlingConnectionStuff;
}

void QnMobileClientUiController::setAvoidHandlingConnectionStuff(bool value)
{
    Q_D(QnMobileClientUiController);
    if (d->avoidHandlingConnectionStuff == value)
        return;

    d->avoidHandlingConnectionStuff = value;
    emit avoidHandlingConnectionStuffChanged();
}

nx::vms::client::mobile::OperationManager* QnMobileClientUiController::operationManager() const
{
    Q_D(const QnMobileClientUiController);
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

void QnMobileClientUiController::openResourcesScreen(const ResourceIdList& filterIds)
{
    NX_DEBUG(this, "openResourcesScreen(): resources count is <%1>", filterIds.size());
    emit resourcesScreenRequested(QVariant::fromValue(filterIds));
}

void QnMobileClientUiController::openVideoScreen(QnResource* cameraResource, qint64 timestamp)
{
    NX_DEBUG(this, "openVideoScreen(): camera is <%1>, timestamp is <%2>",
        (cameraResource ? cameraResource->getId() : nx::Uuid{}), timestamp);
    emit videoScreenRequested(cameraResource, timestamp);
}

void QnMobileClientUiController::openSessionsScreen()
{
    NX_DEBUG(this, "openSessionsScreen(): called");
    emit sessionsScreenRequested();
}
