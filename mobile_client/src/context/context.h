#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <utils/common/instance_storage.h>

class QnConnectionManager;
class QnLoginSessionManager;
class QnColorTheme;
class QnMobileAppInfo;
class QnResolutionUtil;
class QnContextSettings;

class QnContext: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(QnConnectionManager* connectionManager READ connectionManager NOTIFY connectionManagerChanged)
    Q_PROPERTY(QnLoginSessionManager* loginSessionManager READ loginSessionManager NOTIFY loginSessionManagerChanged)
    Q_PROPERTY(QnColorTheme* colorTheme READ colorTheme NOTIFY colorThemeChanged)
    Q_PROPERTY(QnMobileAppInfo* applicationInfo READ applicationInfo NOTIFY applicationInfoChanged)
    Q_PROPERTY(QnContextSettings* settings READ settings NOTIFY settingsChanged)

public:
    QnContext(QObject *parent = NULL);
    virtual ~QnContext();

    QnConnectionManager *connectionManager() const {
        return m_connectionManager;
    }

    QnLoginSessionManager *loginSessionManager() const {
        return m_loginSessionManager;
    }

    QnColorTheme *colorTheme() const {
        return m_colorTheme;
    }

    QnMobileAppInfo *applicationInfo() const {
        return m_appInfo;
    }

    QnContextSettings *settings() const {
        return m_settings;
    }

    QnResolutionUtil *resolutionUtil() const {
        return m_resolutionUtil.data();
    }

    Q_INVOKABLE int dp(qreal dpix) const;
    Q_INVOKABLE int sp(qreal dpix) const;
    Q_INVOKABLE qreal iconScale() const;

    Q_INVOKABLE void enterFullscreen();
    Q_INVOKABLE void exitFullscreen();

    Q_INVOKABLE int getStatusBarHeight() const;
    Q_INVOKABLE int getNavigationBarHeight() const;
    Q_INVOKABLE bool getDeviceIsPhone() const;

signals:
    /* Dummy signals to prevent non-NOTIFYable warnings */
    void connectionManagerChanged();
    void loginSessionManagerChanged();
    void colorThemeChanged();
    void applicationInfoChanged();
    void settingsChanged();

private slots:
    void at_connectionManager_connectedChanged();

private:
    QnConnectionManager *m_connectionManager;
    QnLoginSessionManager *m_loginSessionManager;
    QnColorTheme *m_colorTheme;
    QnMobileAppInfo *m_appInfo;
    QnContextSettings *m_settings;

    QScopedPointer<QnResolutionUtil> m_resolutionUtil;
};

Q_DECLARE_METATYPE(QnContext*)
