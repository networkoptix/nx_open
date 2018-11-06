#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client_core/connection_context_aware.h>
#include <nx/utils/url.h>

class QnConnectionManager;
class QnMobileAppInfo;
class QnCloudStatusWatcher;
class QnMobileClientUiController;
class QnCloudUrlHelper;

namespace nx::vms::client::core {

class UserWatcher;
class TwoWayAudioController;
class OperationManager;

} // namespace nx::vms::client::core

namespace nx::client::mobile { class QmlSettingsAdaptor; }

using nx::client::mobile::QmlSettingsAdaptor;

class QnContext: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(QnConnectionManager* connectionManager MEMBER m_connectionManager CONSTANT)
    Q_PROPERTY(nx::client::mobile::QmlSettingsAdaptor* settings MEMBER m_settings CONSTANT)
    Q_PROPERTY(QnMobileAppInfo* applicationInfo MEMBER m_appInfo CONSTANT)
    Q_PROPERTY(QnCloudStatusWatcher* cloudStatusWatcher READ cloudStatusWatcher CONSTANT)
    Q_PROPERTY(QnMobileClientUiController* uiController READ uiController CONSTANT)
    Q_PROPERTY(nx::vms::client::core::UserWatcher* userWatcher READ userWatcher CONSTANT)
    Q_PROPERTY(nx::vms::client::core::TwoWayAudioController* twoWayAudioController
        READ twoWayAudioController CONSTANT)
    Q_PROPERTY(QnCloudUrlHelper* cloudUrlHelper MEMBER m_cloudUrlHelper CONSTANT)
    Q_PROPERTY(bool liteMode READ liteMode CONSTANT)
    Q_PROPERTY(bool autoLoginEnabled READ autoLoginEnabled WRITE setAutoLoginEnabled
        NOTIFY autoLoginEnabledChanged)
    Q_PROPERTY(bool showCameraInfo READ showCameraInfo WRITE setShowCameraInfo
        NOTIFY showCameraInfoChanged)
    Q_PROPERTY(bool testMode READ testMode CONSTANT)
    Q_PROPERTY(QString initialTest READ initialTest CONSTANT)
    Q_PROPERTY(int deviceStatusBarHeight READ deviceStatusBarHeight
        NOTIFY deviceStatusBarHeightChanged)

    Q_PROPERTY(nx::vms::client::core::OperationManager* operationManager
        READ operationManager CONSTANT)

    Q_PROPERTY(int leftCustomMargin READ leftCustomMargin NOTIFY customMarginsChanged)
    Q_PROPERTY(int rightCustomMargin READ rightCustomMargin NOTIFY customMarginsChanged)
    Q_PROPERTY(int topCustomMargin READ topCustomMargin NOTIFY customMarginsChanged)
    Q_PROPERTY(int bottomCustomMargin READ bottomCustomMargin NOTIFY customMarginsChanged)

public:
    QnContext(QObject *parent = NULL);
    virtual ~QnContext();

    QnMobileClientUiController* uiController() const { return m_uiController; }
    QnCloudStatusWatcher* cloudStatusWatcher() const;
    QnConnectionManager* connectionManager() const;

    nx::vms::client::core::UserWatcher* userWatcher() const;
    nx::vms::client::core::TwoWayAudioController* twoWayAudioController() const;
    nx::vms::client::core::OperationManager* operationManager() const;
    QmlSettingsAdaptor* settings() const;

    Q_INVOKABLE void quitApplication();

    Q_INVOKABLE void enterFullscreen();
    Q_INVOKABLE void exitFullscreen();

    Q_INVOKABLE void copyToClipboard(const QString &text);

    Q_INVOKABLE bool getNavigationBarIsLeftSide() const;
    Q_INVOKABLE int getNavigationBarHeight() const;
    Q_INVOKABLE bool getDeviceIsPhone() const;

    Q_INVOKABLE void setKeepScreenOn(bool keepScreenOn);
    Q_INVOKABLE void setScreenOrientation(Qt::ScreenOrientation orientation);

    Q_INVOKABLE int getMaxTextureSize() const;

    Q_INVOKABLE bool liteMode() const;

    // TODO: #dklychkov Move settings properties to a dedicated object.

    Q_INVOKABLE bool autoLoginEnabled() const;
    Q_INVOKABLE void setAutoLoginEnabled(bool enabled);

    Q_INVOKABLE bool showCameraInfo() const;
    Q_INVOKABLE void setShowCameraInfo(bool showCameraInfo);

    Q_INVOKABLE bool testMode() const;
    Q_INVOKABLE QString initialTest() const;

    Q_INVOKABLE int deviceStatusBarHeight() const;

    Q_INVOKABLE void removeSavedConnection(
        const QString& localSystemId, const QString& userName = QString());

    Q_INVOKABLE void clearSavedPasswords();
    Q_INVOKABLE void clearLastUsedConnection();
    Q_INVOKABLE QString getLastUsedSystemName() const;
    Q_INVOKABLE nx::utils::Url getLastUsedUrl() const;
    Q_INVOKABLE nx::utils::Url getInitialUrl() const;
    Q_INVOKABLE bool isCloudConnectionUrl(const nx::utils::Url& url);
    Q_INVOKABLE nx::utils::Url getWebSocketUrl() const;

    Q_INVOKABLE bool setCloudCredentials(const QString& login, const QString& password);

    Q_INVOKABLE QString lp(const QString& path) const;
    void setLocalPrefix(const QString& prefix);

    Q_INVOKABLE void updateCustomMargins();

    int leftCustomMargin() const;
    int rightCustomMargin() const;
    int topCustomMargin() const;
    int bottomCustomMargin() const;

signals:
    void autoLoginEnabledChanged();
    void showCameraInfoChanged();
    void deviceStatusBarHeightChanged();

    void customMarginsChanged();

private:
    QnConnectionManager *m_connectionManager;
    QmlSettingsAdaptor* m_settings;
    QnMobileAppInfo *m_appInfo;
    QnMobileClientUiController* m_uiController;
    QnCloudUrlHelper* m_cloudUrlHelper;

    QString m_localPrefix;
};

Q_DECLARE_METATYPE(QnContext*)
