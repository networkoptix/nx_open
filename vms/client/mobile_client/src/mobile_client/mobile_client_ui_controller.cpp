// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_ui_controller.h"

#include <context/context.h>
#include <core/resource/resource.h>
#include <mobile_client/mobile_client_settings.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/remote_connection_error.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/session/ui_messages.h>
#include <nx/vms/common/system_settings.h>

class QnMobileClientUiControllerPrivate
{
public:
    QString layoutId;
    QnMobileClientUiController::Screen currentScreen =
        QnMobileClientUiController::Screen::UnknownScreen;
};

QnMobileClientUiController::QnMobileClientUiController(QObject* parent):
    base_type(parent),
    d_ptr(new QnMobileClientUiControllerPrivate())
{
    NX_DEBUG(this, "QnMobileClientUiController(): created mobile client UI controller");

    connect(globalSettings(), &nx::vms::common::SystemSettings::systemNameChanged,
        this, &QnMobileClientUiController::currentSystemNameChanged);
}

QnMobileClientUiController::~QnMobileClientUiController()
{
}

void QnMobileClientUiController::initialize(
    SessionManager* sessionManager,
    QnContext *context)
{
    connect(sessionManager, &SessionManager::hasSessionChanged, this,
        [this, sessionManager]()
        {
            NX_DEBUG(this, "initialize(): hasSessionChanged: screen is <%1>, has session is <%2>",
                currentScreen(), sessionManager->hasSession());

            const auto screen = currentScreen();

            // Custom connection screen manages session presence itself.
            if (screen == Screen::CustomConnectionScreen)
                return;

            if (sessionManager->hasSession())
            {
                openResourcesScreen();
            }
            else
            {
                setLayoutId(QString());
                if (screen != Screen::DigestLoginToCloudScreen)
                    openSessionsScreen();
            }
        });

    connect(sessionManager, &SessionManager::sessionStoppedManually, this,
        [this]()
        {
            NX_DEBUG(this, "initialize(): sessionStoppedManually(): called");
            nx::vms::client::core::appContext()->coreSettings()->lastConnection = {};
        });

    connect(sessionManager, &SessionManager::sessionStartedSuccessfully, this,
        [this]()
        {
            NX_DEBUG(this, "initialize(): sessionStartedSuccessfully(): current screen is <%1>",
                currentScreen());
            if (currentScreen() != Screen::ResourcesScreen)
                emit resourcesScreenRequested(QVariant());
        });

    using Session = nx::vms::client::mobile::Session;
    using RemoteConnectionErrorCode = nx::vms::client::core::RemoteConnectionErrorCode;
    const auto handleSessionError =
        [this, context](
            const Session::Holder& session,
            RemoteConnectionErrorCode status)
        {
            NX_DEBUG(this, "initialize(): sessionFinishedWithError: current screen is <%1>",
                currentScreen());

            // Custom connection screen manages session errors itself.
            if (currentScreen() == Screen::CustomConnectionScreen)
                return;

            nx::vms::client::core::appContext()->coreSettings()->lastConnection = {};

            using UiMessages = nx::vms::client::mobile::UiMessages;
            const auto errorText = UiMessages::getConnectionErrorText(status);
            context->showConnectionErrorMessage(session->systemName(), errorText);
        };
    connect(sessionManager, &SessionManager::sessionFinishedWithError,
        context, handleSessionError);
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

QString QnMobileClientUiController::layoutId() const
{
    Q_D(const QnMobileClientUiController);
    return d->layoutId;
}

void QnMobileClientUiController::setLayoutId(const QString& layoutId)
{
    Q_D(QnMobileClientUiController);
    if (d->layoutId == layoutId)
        return;

    d->layoutId = layoutId;
    emit layoutIdChanged();
}

QString QnMobileClientUiController::currentSystemName() const
{
    return globalSettings()->systemName();
}

void QnMobileClientUiController::openConnectToServerScreen(
    const nx::utils::Url& url,
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
