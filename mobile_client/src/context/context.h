#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <utils/common/instance_storage.h>

class QnConnectionManager;
class QnColorTheme;
class QnMobileAppInfo;
class QnContextSettings;

class QnContext: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(QnConnectionManager* connectionManager READ connectionManager NOTIFY connectionManagerChanged)
    Q_PROPERTY(QnColorTheme* colorTheme READ colorTheme NOTIFY colorThemeChanged)
    Q_PROPERTY(QnMobileAppInfo* applicationInfo READ applicationInfo NOTIFY applicationInfoChanged)
    Q_PROPERTY(QnContextSettings* settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(bool liteMode READ liteMode NOTIFY liteModeChanged)

public:
    QnContext(QObject *parent = NULL);
    virtual ~QnContext();

    QnConnectionManager *connectionManager() const {
        return m_connectionManager;
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

    Q_INVOKABLE void removeSavedConnection(const QString& systemName);

    Q_INVOKABLE void setLastUsedConnection(const QString& systemId, const QUrl& url);
    Q_INVOKABLE void clearLastUsedConnection();
    Q_INVOKABLE QString getLastUsedSystemId() const;
    Q_INVOKABLE QString getLastUsedUrl() const;

    Q_INVOKABLE QString lp(const QString& path) const;
    void setLocalPrefix(const QString& prefix);

signals:
    /* Dummy signals to prevent non-NOTIFYable warnings */
    void connectionManagerChanged();
    void colorThemeChanged();
    void applicationInfoChanged();
    void settingsChanged();
    void liteModeChanged();

private:
    QnConnectionManager *m_connectionManager;
    QnColorTheme *m_colorTheme;
    QnMobileAppInfo *m_appInfo;
    QnContextSettings *m_settings;

    QString m_localPrefix;
};

Q_DECLARE_METATYPE(QnContext*)
