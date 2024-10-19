// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMargins>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <nx/utils/url.h>
#include <nx/vms/client/mobile/system_context_aware.h>

Q_MOC_INCLUDE("mobile_client/mobile_client_ui_controller.h")
Q_MOC_INCLUDE("nx/mobile_client/controllers/audio_controller.h")
Q_MOC_INCLUDE("nx/vms/client/core/common/utils/cloud_url_helper.h")
Q_MOC_INCLUDE("nx/vms/client/core/event_search/models/event_search_model_adapter.h")
Q_MOC_INCLUDE("nx/vms/client/core/network/cloud_status_watcher.h")
Q_MOC_INCLUDE("nx/vms/client/core/two_way_audio/two_way_audio_controller.h")
Q_MOC_INCLUDE("nx/vms/client/core/two_way_audio/two_way_audio_controller.h")
Q_MOC_INCLUDE("nx/vms/client/core/watchers/user_watcher.h")
Q_MOC_INCLUDE("nx/vms/client/core/watchers/watermark_watcher.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/push_notification/push_notification_manager.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/session/session_manager.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/utils/operation_manager.h")
Q_MOC_INCLUDE("settings/qml_settings_adaptor.h")
Q_MOC_INCLUDE("utils/mobile_app_info.h")

class QnMobileAppInfo;
class QnMobileClientUiController;

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::core {

class CloudStatusWatcher;
class CloudUrlHelper;
class OauthClient;
class ServerTimeWatcher;
class TwoWayAudioController;
class UserWatcher;
class WatermarkWatcher;
class CommonObjectSearchSetup;
class AnalyticsSearchSetup;
class EventSearchModelAdapter;

} // namespace nx::vms::client::core

using nx::vms::client::core::ServerTimeWatcher;

namespace nx::client::mobile {
class QmlSettingsAdaptor;
} // namespace nx::client::mobile

namespace nx::vms::client::mobile {

class AudioController;
class SessionManager;
class PushNotificationManager;
class OperationManager;

namespace details {
class PushSystemsSelectionModel;
} // namespace details

} // namespace nx::vms::client::mobile

using nx::client::mobile::QmlSettingsAdaptor;
using nx::vms::client::mobile::AudioController;

class QnContext: public QObject, public nx::vms::client::mobile::SystemContextAware
{
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(nx::vms::client::mobile::SessionManager* sessionManager
        READ sessionManager CONSTANT)

    Q_PROPERTY(nx::vms::client::mobile::AudioController* audioController
        MEMBER m_audioController CONSTANT)
    Q_PROPERTY(nx::client::mobile::QmlSettingsAdaptor* settings MEMBER m_settings CONSTANT)
    Q_PROPERTY(QnMobileAppInfo* applicationInfo MEMBER m_appInfo CONSTANT)
    Q_PROPERTY(nx::vms::client::core::CloudStatusWatcher* cloudStatusWatcher
        READ cloudStatusWatcher CONSTANT)
    Q_PROPERTY(QnMobileClientUiController* uiController READ uiController CONSTANT)
    Q_PROPERTY(nx::vms::client::core::WatermarkWatcher* watermarkWatcher
        READ watermarkWatcher
        CONSTANT)
    Q_PROPERTY(nx::vms::client::core::UserWatcher* currentUserWatcher READ currentUserWatcher CONSTANT)
    Q_PROPERTY(nx::vms::client::core::TwoWayAudioController* twoWayAudioController
        READ twoWayAudioController CONSTANT)
    Q_PROPERTY(nx::vms::client::core::CloudUrlHelper* cloudUrlHelper
        MEMBER m_cloudUrlHelper
        CONSTANT)
    Q_PROPERTY(bool showCameraInfo READ showCameraInfo WRITE setShowCameraInfo
        NOTIFY showCameraInfoChanged)
    Q_PROPERTY(int deviceStatusBarHeight READ deviceStatusBarHeight
        NOTIFY deviceStatusBarHeightChanged)

    Q_PROPERTY(nx::vms::client::mobile::OperationManager* operationManager
        READ operationManager CONSTANT)

    Q_PROPERTY(int leftCustomMargin READ leftCustomMargin NOTIFY customMarginsChanged)
    Q_PROPERTY(int rightCustomMargin READ rightCustomMargin NOTIFY customMarginsChanged)
    Q_PROPERTY(int topCustomMargin READ topCustomMargin NOTIFY customMarginsChanged)
    Q_PROPERTY(int bottomCustomMargin READ bottomCustomMargin NOTIFY customMarginsChanged)

    Q_PROPERTY(bool serverTimeMode READ serverTimeMode WRITE setServerTimeMode
        NOTIFY serverTimeModeChanged)

    Q_PROPERTY(nx::vms::client::mobile::PushNotificationManager* pushManager
        MEMBER m_pushNotificationManager CONSTANT)

    Q_PROPERTY(bool hasDigestCloudPassword
        READ hasDigestCloudPassword
        NOTIFY digestCloudPasswordChanged)

    /**
     * Workaround for the QTBUG-72472 - view is not changing size if there is Android WebView on the
     * scene. Also keyboard height is always 0 in this situation in Qt.
     * Check if the bug is fixed in Qt 6.x.
     */
    Q_PROPERTY(int androidKeyboardHeight
        READ androidKeyboardHeight
        NOTIFY androidKeyboardHeightChanged)

public:
    QnContext(nx::vms::client::mobile::SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~QnContext();

    void initCloudStatusHandling();

    QnMobileClientUiController* uiController() const { return m_uiController; }
    nx::vms::client::core::CloudStatusWatcher* cloudStatusWatcher() const;

    nx::vms::client::core::UserWatcher* currentUserWatcher() const;
    nx::vms::client::core::WatermarkWatcher* watermarkWatcher() const;
    nx::vms::client::core::TwoWayAudioController* twoWayAudioController() const;
    nx::vms::client::mobile::OperationManager* operationManager() const;
    QmlSettingsAdaptor* settings() const;

    Q_INVOKABLE QObject* context();

    Q_INVOKABLE void quitApplication();

    Q_INVOKABLE void enterFullscreen();
    Q_INVOKABLE void exitFullscreen();

    Q_INVOKABLE void copyToClipboard(const QString &text);

    Q_INVOKABLE int navigationBarType() const;
    Q_INVOKABLE int getNavigationBarSize() const;
    Q_INVOKABLE bool getDeviceIsPhone() const;

    Q_INVOKABLE void setKeepScreenOn(bool keepScreenOn);
    Q_INVOKABLE void setScreenOrientation(Qt::ScreenOrientation orientation);

    Q_INVOKABLE int getMaxTextureSize() const;

    // TODO: #dklychkov Move settings properties to a dedicated object.

    Q_INVOKABLE bool showCameraInfo() const;
    Q_INVOKABLE void setShowCameraInfo(bool showCameraInfo);

    Q_INVOKABLE int deviceStatusBarHeight() const;

    Q_INVOKABLE void removeSavedConnection(
        const QString& systemId,
        const QString& localSystemId,
        const QString& userName = QString());

    Q_INVOKABLE void removeSavedCredentials(
        const QString& localSystemId,
        const QString& userName);

    Q_INVOKABLE void removeSavedAuth(
        const QString& localSystemId,
        const QString& userName);

    Q_INVOKABLE void clearSavedPasswords();

    Q_INVOKABLE QString lp(const QString& path) const;
    void setLocalPrefix(const QString& prefix);

    Q_INVOKABLE void updateCustomMargins();

    Q_INVOKABLE void makeShortVibration();

    Q_INVOKABLE nx::vms::client::mobile::details::PushSystemsSelectionModel*
        createPushSelectionModel() const;

    Q_INVOKABLE void showConnectionErrorMessage(
        const QString& systemName,
        const QString& errorText);

    Q_INVOKABLE void openExternalLink(const QUrl& url);

    int leftCustomMargin() const;
    int rightCustomMargin() const;
    int topCustomMargin() const;
    int bottomCustomMargin() const;

    bool serverTimeMode() const;
    void setServerTimeMode(bool value);

    nx::vms::client::mobile::SessionManager* sessionManager() const;

    /** Create Oauth client with the specfied token and optional user. */
    Q_INVOKABLE nx::vms::client::core::OauthClient* createOauthClient(
        const QString& token,
        const QString& user) const;

    /** Return current url of the Web Kit window. Used in workaround for iOS platform. */
    Q_INVOKABLE QString getWebkitUrl() const;

    int androidKeyboardHeight() const;

    /** Show 2FA validation screen and validate specified token. */
    bool show2faValidationScreen(const nx::network::http::Credentials& credentials);

    /** Check saved properties and try to restore last used connection. */
    Q_INVOKABLE bool tryRestoreLastUsedConnection();

    bool hasDigestCloudPassword() const;

    Q_INVOKABLE void requestRecordAudioPermissionIfNeeded();

    Q_INVOKABLE nx::vms::client::core::EventSearchModelAdapter* createSearchModel(bool analyticsMode);

    Q_INVOKABLE nx::vms::client::core::CommonObjectSearchSetup* createSearchSetup(
        nx::vms::client::core::EventSearchModelAdapter* model);

    Q_INVOKABLE nx::vms::client::core::AnalyticsSearchSetup* createAnalyticsSearchSetup(
        nx::vms::client::core::EventSearchModelAdapter* model);

    Q_INVOKABLE QString plainText(const QString& value);

    Q_INVOKABLE bool hasViewBookmarksPermission();

    Q_INVOKABLE bool hasSearchObjectsPermission();

    Q_INVOKABLE QString qualityText(const QSize& resolution, int quality);

    Q_INVOKABLE void setGestureExclusionArea(int y, int height);

signals:
    void showMessage(
        const QString& caption,
        const QString& description);
    void serverTimeModeChanged();
    void showCameraInfoChanged();
    void deviceStatusBarHeightChanged();

    void customMarginsChanged();
    void showLinksChanged();
    void digestCloudPasswordChanged();
    void androidKeyboardHeightChanged();

private:
    AudioController* const m_audioController;
    QmlSettingsAdaptor* const m_settings;
    QnMobileAppInfo* const m_appInfo;
    QnMobileClientUiController* const m_uiController;
    nx::vms::client::core::CloudUrlHelper* const m_cloudUrlHelper;
    ServerTimeWatcher* const m_timeWatcher;
    QString m_localPrefix;
    QMargins m_customMargins;

    nx::vms::client::mobile::PushNotificationManager* const m_pushNotificationManager;
    int m_androidKeyboardHeight = 0;
    nx::vms::client::mobile::OperationManager* const m_operationManager;
    nx::vms::client::core::TwoWayAudioController* const m_twoWayAudioController;
};

Q_DECLARE_METATYPE(QnContext*)
