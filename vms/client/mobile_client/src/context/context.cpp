// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "context.h"

#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QTextDocumentFragment>

#include <camera/camera_thumbnail_cache.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <mobile_client/mobile_client_module.h>
#include <mobile_client/mobile_client_settings.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/media/media_player.h>
#include <nx/mobile_client/controllers/audio_controller.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/core/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/core/event_search/models/event_search_model_adapter.h>
#include <nx/vms/client/core/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/network/oauth_client.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/controllers/web_view_controller.h>
#include <nx/vms/client/mobile/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/mobile/push_notification/details/push_systems_selection_model.h>
#include <nx/vms/client/mobile/push_notification/push_notification_manager.h>
#include <nx/vms/client/mobile/session/session.h>
#include <nx/vms/client/mobile/session/session_manager.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/client/mobile/ui/qml_wrapper_helper.h>
#include <nx/vms/client/mobile/utils/navigation_bar_utils.h>
#include <nx/vms/client/mobile/utils/operation_manager.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/time/formatter.h>
#include <settings/qml_settings_adaptor.h>
#include <ui/texture_size_helper.h>
#include <ui/window_utils.h>
#include <utils/common/delayed.h>
#include <utils/mobile_app_info.h>
#include <watchers/available_cameras_watcher.h>

using namespace nx::vms::utils;
using namespace nx::vms::client::core;
using namespace nx::vms::client::mobile;

namespace {

static const nx::utils::SoftwareVersion kUserRightsRefactoredVersion(3, 0);

bool isCloudUrl(const nx::utils::Url& url)
{
    return url.scheme() == Session::kCloudUrlScheme
        || nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());
}

Settings* coreSettings()
{
    return nx::vms::client::core::appContext()->coreSettings();
}

} // namespace

QnContext::QnContext(nx::vms::client::mobile::SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    m_audioController(new AudioController(this)),
    m_settings(new QmlSettingsAdaptor(this)),
    m_appInfo(new QnMobileAppInfo(this)),
    m_uiController(new QnMobileClientUiController(this)),
    m_cloudUrlHelper(new CloudUrlHelper(
        SystemUri::ReferralSource::MobileClient,
        SystemUri::ReferralContext::WelcomePage,
        this)),
    m_timeWatcher(systemContext->serverTimeWatcher()),
    m_localPrefix(lit("qrc:///")),
    m_customMargins(),
    m_pushNotificationManager(qnMobileClientModule->pushManager()),
    m_operationManager(new OperationManager(this)),
    m_twoWayAudioController(new TwoWayAudioController(systemContext, this))
{
    m_uiController->initialize(sessionManager(), this);

    const auto screen = qApp->primaryScreen();
    // TODO: Qt6 Removed: https://bugreports.qt.io/browse/QTBUG-83055
    // screen->setOrientationUpdateMask(Qt::PortraitOrientation | Qt::InvertedPortraitOrientation
    //     | Qt::LandscapeOrientation | Qt::InvertedLandscapeOrientation);
    connect(screen, &QScreen::orientationChanged, this,
        [this]()
        {
            const auto emitDeviceStatusBarHeightChanged = nx::utils::guarded(this,
                [this]() { emit deviceStatusBarHeightChanged(); });
            const auto kAnimationDelayMs = std::chrono::milliseconds(300);
            executeDelayedParented(emitDeviceStatusBarHeightChanged, kAnimationDelayMs, this);
        });

    connect(sessionManager(), &SessionManager::stateChanged, this,
        [this]()
        {
            auto thumbnailCache = QnCameraThumbnailCache::instance();
            if (!thumbnailCache)
                return;

            if (sessionManager()->hasConnectedSession())
                thumbnailCache->start();
            else
                thumbnailCache->stop();
        });

    connect(sessionManager(), &SessionManager::connectedServerVersionChanged, this,
        [this]()
        {
            const bool compatibilityMode =
                sessionManager()->connectedServerVersion() < kUserRightsRefactoredVersion;
            const auto camerasWatcher = qnMobileClientModule->availableCamerasWatcher();
            camerasWatcher->setCompatiblityMode(compatibilityMode);
        });


    const auto updateTrafficLogging =
        []()
        {
            nx::network::http::AsyncClient::setForceTrafficLogging(
                qnSettings->forceTrafficLogging());
        };
    updateTrafficLogging();

    connect(qnSettings, &QnMobileClientSettings::valueChanged, this,
        [this, updateTrafficLogging](int id)
        {
            switch (id)
            {
                case QnMobileClientSettings::ShowCameraInfo:
                    emit showCameraInfoChanged();
                    break;
                case QnMobileClientSettings::ForceTrafficLogging:
                    updateTrafficLogging();
                    break;
                default:
                    break;
            };
        });

    auto updateMarginsCallback =
        [this]()
        {
            // We have to update margins after all screen-change animations are finished.
            static constexpr int kUpdateDelayMs = 300;
            const auto callback = [this]() { updateCustomMargins(); };
            executeDelayedParented(nx::utils::guarded(this, callback), kUpdateDelayMs, this);
        };

    connect(qApp->screens().first(), &QScreen::geometryChanged, this, updateMarginsCallback);

    const auto userWatcher = systemContext->userWatcher();
    connect(userWatcher, &UserWatcher::userNameChanged, this,
        [this, userWatcher]()
        {
            if (userWatcher->userName().isEmpty())
            {
                NX_DEBUG(this, "User name changed to empty, stopping session");
                sessionManager()->stopSession();
            }
        });

    const auto updateTimeMode =
        [this]()
        {
            m_timeWatcher->setTimeMode(qnSettings->serverTimeMode()
                ? ServerTimeWatcher::serverTimeMode
                : ServerTimeWatcher::clientTimeMode);
        };
    const auto notifier = qnSettings->notifier(QnMobileClientSettings::ServerTimeMode);
    connect(notifier, &QnPropertyNotifier::valueChanged, this, updateTimeMode);
    connect(m_timeWatcher, &ServerTimeWatcher::timeZoneChanged,
        this, &QnContext::serverTimeModeChanged);
    updateTimeMode();

    using SecureProperty = nx::utils::property_storage::SecureProperty<std::string>;
    connect(&coreSettings()->digestCloudPassword, &SecureProperty::changed,
        this, &QnContext::digestCloudPasswordChanged);

    if (nx::build_info::isAndroid())
    {
        const auto timer = new QTimer(this);
        timer->setInterval(std::chrono::milliseconds(100));
        connect(timer, &QTimer::timeout, timer,
            [this]()
            {
                const int value = ::androidKeyboardHeight();
                if (value == m_androidKeyboardHeight)
                    return;

                m_androidKeyboardHeight = value;
                emit androidKeyboardHeightChanged();
            });
        timer->start();
    }

    if (nx::build_info::isIos())
    {
        /**
         * Workaround for iOS 16.x - there are no screen orientation change update when we
         * forcing rotation.
         */
        const auto timer = new QTimer(this);
        timer->setInterval(std::chrono::milliseconds(500));
        connect(timer, &QTimer::timeout, this, &QnContext::deviceStatusBarHeightChanged);
        timer->start();
    }

    initCloudStatusHandling();
}

QnContext::~QnContext()
{
}

void QnContext::initCloudStatusHandling()
{
    using namespace nx::vms::client;

    const auto watcher = cloudStatusWatcher();
    connect(watcher, &CloudStatusWatcher::statusChanged, this,
        [this, watcher]()
        {
            if (watcher->status() != CloudStatusWatcher::LoggedOut)
                return;

            coreSettings()->digestCloudPassword = {};

            const auto& connectionData = coreSettings()->lastConnection();
            if (isCloudUrl(connectionData.url))
                sessionManager()->stopSession();
        });

    connect(watcher, &CloudStatusWatcher::forcedLogout, this,
        [this]()
        {
            const auto description =
                core::errorDescription(RemoteConnectionErrorCode::cloudSessionExpired, {}, {});
            showMessage(description.shortText, description.longText);
        });
}

CloudStatusWatcher* QnContext::cloudStatusWatcher() const
{
    return nx::vms::client::core::appContext()->cloudStatusWatcher();
}

TwoWayAudioController* QnContext::twoWayAudioController() const
{
    return m_twoWayAudioController;
}

OperationManager* QnContext::operationManager() const
{
    return m_operationManager;
}

UserWatcher* QnContext::currentUserWatcher() const
{
    return systemContext()->userWatcher();
}

WatermarkWatcher* QnContext::watermarkWatcher() const
{
    return systemContext()->watermarkWatcher();
}

QmlSettingsAdaptor* QnContext::settings() const
{
    return m_settings;
}

QObject* QnContext::context()
{
    return this;
}

void QnContext::quitApplication()
{
    qApp->quit();
}

void QnContext::exitFullscreen()
{
    showSystemUi();
    emit deviceStatusBarHeightChanged();
}

void QnContext::copyToClipboard(const QString &text)
{
    qApp->clipboard()->setText(text);
}

void QnContext::enterFullscreen()
{
    hideSystemUi();
    emit deviceStatusBarHeightChanged();
}

int QnContext::deviceStatusBarHeight() const
{
    return ::statusBarHeight();
}

int QnContext::navigationBarType() const
{
    return NavigationBarUtils::navigationBarType();
}

int QnContext::getNavigationBarSize() const
{
    return NavigationBarUtils::barSize();
}

bool QnContext::getDeviceIsPhone() const {
    return isPhone();
}

void QnContext::setKeepScreenOn(bool keepScreenOn) {
    ::setKeepScreenOn(keepScreenOn);
}

void QnContext::setScreenOrientation(Qt::ScreenOrientation orientation)
{
    ::setScreenOrientation(orientation);
}

int QnContext::getMaxTextureSize() const
{
    if (const auto textureSizeHelper = QnTextureSizeHelper::instance())
        return textureSizeHelper->maxTextureSize();

    return -1;
}

bool QnContext::showCameraInfo() const
{
    return qnSettings->showCameraInfo();
}

void QnContext::setShowCameraInfo(bool showCameraInfo)
{
    if (showCameraInfo == qnSettings->showCameraInfo())
        return;

    qnSettings->setShowCameraInfo(showCameraInfo);
}

void QnContext::removeSavedConnection(
    const QString& systemId,
    const QString& localSystemId,
    const QString& userName)
{
    const auto localId = nx::Uuid::fromStringSafe(localSystemId);

    NX_ASSERT(!localId.isNull());
    if (localId.isNull())
        return;

    CredentialsManager::removeCredentials(localId, userName.toStdString());

    if (userName.isEmpty() || CredentialsManager::credentials(localId).empty())
    {
        coreSettings()->removeRecentConnection(localId);
        nx::vms::client::core::appContext()->knownServerConnectionsWatcher()->removeSystem(systemId);
    }
}

void QnContext::removeSavedCredentials(
    const QString& localSystemId,
    const QString& userName)
{
    if (!NX_ASSERT(!localSystemId.isNull() && !userName.isEmpty()))
        return;

    const auto localId = nx::Uuid::fromStringSafe(localSystemId);
    CredentialsManager::removeCredentials(localId, userName.toStdString());
}

void QnContext::removeSavedAuth(
    const QString& localSystemId,
    const QString& userName)
{
    if (!NX_ASSERT(!localSystemId.isNull() && !userName.isEmpty()))
        return;

    const auto localId = nx::Uuid::fromStringSafe(localSystemId);
    const auto emptyAuthCredentials = nx::network::http::Credentials{userName.toStdString(), {}};
    CredentialsManager::storeCredentials(localId, emptyAuthCredentials);
}

void QnContext::clearSavedPasswords()
{
    CredentialsManager::forgetStoredPasswords();
}

QString QnContext::lp(const QString& path) const
{
    if (path.isEmpty())
        return path;

    return m_localPrefix + path;
}

void QnContext::setLocalPrefix(const QString& prefix)
{
    m_localPrefix = prefix;
    if (!m_localPrefix.endsWith('/'))
        m_localPrefix.append('/');
}

void QnContext::updateCustomMargins()
{
    const auto newMargins = getCustomMargins();
    if (m_customMargins == newMargins)
        return;

    m_customMargins = newMargins;
    emit customMarginsChanged();
}

void QnContext::makeShortVibration()
{
    ::makeShortVibration();
}

details::PushSystemsSelectionModel* QnContext::createPushSelectionModel() const
{
    // No parent means Qml will take care of it.
    return new details::PushSystemsSelectionModel(
        cloudStatusWatcher()->recentCloudSystems(),
        m_pushNotificationManager->selectedSystems());
}

void QnContext::showConnectionErrorMessage(
    const QString& systemName,
    const QString& errorText)
{
    const auto title = systemName.isEmpty()
            ? tr("Cannot connect to the Server")
            : tr("Cannot connect to the Site \"%1\"", "%1 is a site name").arg(systemName);
    showMessage(title, errorText);
}


void QnContext::openExternalLink(const QUrl& url)
{
    if (const auto webView = qnMobileClientModule->webViewController())
        webView->openUrl(url);
    else
        QDesktopServices::openUrl(url);
}

int QnContext::leftCustomMargin() const
{
    return m_customMargins.left();
}

int QnContext::rightCustomMargin() const
{
    return m_customMargins.right();
}

int QnContext::topCustomMargin() const
{
    return m_customMargins.top();
}

int QnContext::bottomCustomMargin() const
{
    return m_customMargins.bottom();
}

bool QnContext::serverTimeMode() const
{
    return m_timeWatcher->timeMode() == ServerTimeWatcher::serverTimeMode;
}

void QnContext::setServerTimeMode(bool value)
{
    qnSettings->setServerTimeMode(value);
}

SessionManager* QnContext::sessionManager() const
{
    return systemContext()->sessionManager();
}

OauthClient* QnContext::createOauthClient(
    const QString& token,
    const QString& user) const
{
    OauthClientType type = token.isEmpty()
        ? OauthClientType::loginCloud
        : OauthClientType::renewDesktop;

    static constexpr std::chrono::days kRefreshTokenLifetime(180);
    const auto result = new OauthClient(type, OauthViewType::mobile, /*cloudSystem*/ {},
        kRefreshTokenLifetime);

    if (!token.isEmpty())
        result->setCredentials(nx::network::http::Credentials(
            user.toStdString(), nx::network::http::BearerAuthToken(token.toStdString())));

    connect(result, &OauthClient::authDataReady, this,
        [this, result]()
        {
            const auto authData = result->authData();
            if (authData.empty())
                emit result->cancelled();
            else
                cloudStatusWatcher()->setAuthData(authData, CloudStatusWatcher::AuthMode::login);
        });
    return result;
}

QString QnContext::getWebkitUrl() const
{
    return ::webkitUrl();
}

int QnContext::androidKeyboardHeight() const
{
    return m_androidKeyboardHeight;
}

bool QnContext::show2faValidationScreen(const nx::network::http::Credentials& credentials)
{
    QVariantMap properties;
    properties["token"] = QString::fromStdString(credentials.authToken.value);
    return QmlWrapperHelper::showScreen(
        QUrl("qrc:qml/Nx/Screens/Cloud/Login.qml"), properties) == "success";
}

bool QnContext::tryRestoreLastUsedConnection()
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

    const auto& connectionData = coreSettings()->lastConnection();
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

    if (cloudStatusWatcher()->status() != CloudStatusWatcher::LoggedOut)
    {
        NX_DEBUG(this, "Restoring last used cloud session.");
        sessionManager()->startCloudSession(systemId.toSimpleString());
        return true;
    }

    NX_DEBUG(this, "Can't restore last used session since user is not logged to the cloud.");
    return false;
}

bool QnContext::hasDigestCloudPassword() const
{
    return !coreSettings()->digestCloudPassword().empty();
}

void QnContext::requestRecordAudioPermissionIfNeeded()
{
    ::requestRecordAudioPermissionIfNeeded();
}

EventSearchModelAdapter* QnContext::createSearchModel(
    bool analyticsMode)
{
    const auto context = systemContext();
    const auto adapter = new EventSearchModelAdapter();
    adapter->setSearchModel(analyticsMode
        ? static_cast<AbstractAsyncSearchListModel*>(new AnalyticsSearchListModel(context, adapter))
        : static_cast<AbstractAsyncSearchListModel*>(new BookmarkSearchListModel(context, adapter)));
    return adapter;
}

nx::vms::client::core::CommonObjectSearchSetup* QnContext::createSearchSetup(
    EventSearchModelAdapter* model)
{
    const auto result = new nx::vms::client::mobile::CommonObjectSearchSetup(systemContext());
    if (model)
        result->setModel(model->searchModel());
    return result;
}

AnalyticsSearchSetup* QnContext::createAnalyticsSearchSetup(
    EventSearchModelAdapter* model)
{
    if (!model)
        return nullptr;

    if (const auto analyticsModel = qobject_cast<AnalyticsSearchListModel*>(model->searchModel()))
        return new AnalyticsSearchSetup(analyticsModel);

    return nullptr;
}

QString QnContext::plainText(const QString& value)
{
    return QTextDocumentFragment::fromHtml(value).toPlainText();
}

bool QnContext::hasViewBookmarksPermission()
{
    return accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewBookmarks);
}

bool QnContext::hasSearchObjectsPermission()
{
    static const nx::utils::SoftwareVersion kObjectSearchMinimalSupportVersion(5, 0);

    const auto context = systemContext();
    const auto server = context->currentServer();
    return server
        && server->getVersion() >= kObjectSearchMinimalSupportVersion
        && context->accessController()->isDeviceAccessRelevant(nx::vms::api::AccessRight::viewArchive);
}

QString QnContext::qualityText(const QSize& resolution, int quality)
{
    if (resolution.width() > 0 && resolution.height() > 0)
        return QString("%1x%2").arg(resolution.width()).arg(resolution.height());

    if (quality != nx::media::Player::VideoQuality::LowVideoQuality
        && quality != nx::media::Player::VideoQuality::HighVideoQuality)
    {
        return QString("%1p").arg(quality);
    }
    return tr("Unknown");
}

void QnContext::setGestureExclusionArea(int y, int height)
{
    ::setAndroidGestureExclusionArea(y, height);
}
