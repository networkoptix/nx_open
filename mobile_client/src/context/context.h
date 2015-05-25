#ifndef CONTEXT_H
#define CONTEXT_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <utils/common/instance_storage.h>

class QnConnectionManager;
class QnColorTheme;
class QnMobileAppInfo;

class QnContext: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(QnConnectionManager* connectionManager READ connectionManager NOTIFY connectionManagerChanged)
    Q_PROPERTY(QnColorTheme* colorTheme READ colorTheme NOTIFY colorThemeChanged)
    Q_PROPERTY(QnMobileAppInfo* applicationInfo READ applicationInfo NOTIFY applicationInfoChanged)
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

signals:
    /* Dummy signals to prevent non-NOTIFYable warnings */
    void connectionManagerChanged();
    void colorThemeChanged();
    void applicationInfoChanged();

private slots:
    void at_connectionManager_connected();
    void at_connectionManager_disconnected();

private:
    QnConnectionManager *m_connectionManager;
    QnColorTheme *m_colorTheme;
    QnMobileAppInfo *m_appInfo;
};

Q_DECLARE_METATYPE(QnContext*)

#endif // CONTEXT_H
