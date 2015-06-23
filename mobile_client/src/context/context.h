#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <utils/common/instance_storage.h>

class QnConnectionManager;
class QnLoginSessionManager;
class QnColorTheme;
class QnMobileAppInfo;
class QnResolutionUtil;

class QnContext: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(QnConnectionManager* connectionManager READ connectionManager NOTIFY connectionManagerChanged)
    Q_PROPERTY(QnLoginSessionManager* loginSessionManager READ loginSessionManager NOTIFY loginSessionManagerChanged)
    Q_PROPERTY(QnColorTheme* colorTheme READ colorTheme NOTIFY colorThemeChanged)
    Q_PROPERTY(QnMobileAppInfo* applicationInfo READ applicationInfo NOTIFY applicationInfoChanged)

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

    Q_INVOKABLE int dp(qreal dpix) const;
    Q_INVOKABLE int sp(qreal dpix) const;

signals:
    /* Dummy signals to prevent non-NOTIFYable warnings */
    void connectionManagerChanged();
    void loginSessionManagerChanged();
    void colorThemeChanged();
    void applicationInfoChanged();

private slots:
    void at_connectionManager_connectedChanged();

private:
    QnConnectionManager *m_connectionManager;
    QnLoginSessionManager *m_loginSessionManager;
    QnColorTheme *m_colorTheme;
    QnMobileAppInfo *m_appInfo;

    QScopedPointer<QnResolutionUtil> m_resolutionUtil;
};

Q_DECLARE_METATYPE(QnContext*)
