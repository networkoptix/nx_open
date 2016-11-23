#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

class QnConnectionManager;
class QnMobileAppInfo;
class QnCloudStatusWatcher;
class QnMobileClientUiController;
class QnUserWatcher;
class QnCloudUrlHelper;
class NxGlobalsObject;

class QnContext: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(NxGlobalsObject* Nx MEMBER m_nxGlobals CONSTANT)
    Q_PROPERTY(QnConnectionManager* connectionManager MEMBER m_connectionManager CONSTANT)
    Q_PROPERTY(QnMobileAppInfo* applicationInfo MEMBER m_appInfo CONSTANT)
    Q_PROPERTY(QnCloudStatusWatcher* cloudStatusWatcher READ cloudStatusWatcher CONSTANT)
    Q_PROPERTY(QnMobileClientUiController* uiController READ uiController CONSTANT)
    Q_PROPERTY(QnUserWatcher* userWatcher READ userWatcher CONSTANT)
    Q_PROPERTY(QnCloudUrlHelper* cloudUrlHelper MEMBER m_cloudUrlHelper CONSTANT)
    Q_PROPERTY(bool liteMode READ liteMode CONSTANT)
    Q_PROPERTY(bool autoLoginEnabled READ autoLoginEnabled WRITE setAutoLoginEnabled
        NOTIFY autoLoginEnabledChanged)
    Q_PROPERTY(bool testMode READ testMode CONSTANT)
    Q_PROPERTY(QString initialTest READ initialTest CONSTANT)

public:
    QnContext(QObject *parent = NULL);
    virtual ~QnContext();

    QnMobileClientUiController* uiController() const { return m_uiController; }
    QnCloudStatusWatcher* cloudStatusWatcher() const;
    QnUserWatcher* userWatcher() const;

    Q_INVOKABLE void quitApplication();

    Q_INVOKABLE void enterFullscreen();
    Q_INVOKABLE void exitFullscreen();

    Q_INVOKABLE void copyToClipboard(const QString &text);

    Q_INVOKABLE int getStatusBarHeight() const;
    Q_INVOKABLE int getNavigationBarHeight() const;
    Q_INVOKABLE bool getDeviceIsPhone() const;

    Q_INVOKABLE void setKeepScreenOn(bool keepScreenOn);
    Q_INVOKABLE void setScreenOrientation(Qt::ScreenOrientation orientation);

    Q_INVOKABLE int getMaxTextureSize() const;

    Q_INVOKABLE bool liteMode() const;

    Q_INVOKABLE bool autoLoginEnabled() const;
    Q_INVOKABLE void setAutoLoginEnabled(bool enabled);

    Q_INVOKABLE bool testMode() const;
    Q_INVOKABLE QString initialTest() const;

    Q_INVOKABLE void removeSavedConnection(const QString& systemName);

    Q_INVOKABLE void clearLastUsedConnection();
    Q_INVOKABLE QString getLastUsedSystemName() const;
    Q_INVOKABLE QUrl getLastUsedUrl() const;
    Q_INVOKABLE QUrl getInitialUrl() const;

    Q_INVOKABLE QUrl getWebSocketUrl() const;

    Q_INVOKABLE void setCloudCredentials(const QString& login, const QString& password);

    Q_INVOKABLE QString lp(const QString& path) const;
    void setLocalPrefix(const QString& prefix);

signals:
    bool autoLoginEnabledChanged();

private:
    NxGlobalsObject* m_nxGlobals;
    QnConnectionManager *m_connectionManager;
    QnMobileAppInfo *m_appInfo;
    QnMobileClientUiController* m_uiController;
    QnCloudUrlHelper* m_cloudUrlHelper;

    QString m_localPrefix;
};

Q_DECLARE_METATYPE(QnContext*)
