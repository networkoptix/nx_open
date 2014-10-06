#ifndef CONTEXT_H
#define CONTEXT_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <utils/common/instance_storage.h>

class QnConnectionManager;
class QnColorTheme;

class QnContext: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(QnConnectionManager* connectionManager READ connectionManager NOTIFY connectionManagerChanged)
    Q_PROPERTY(QnColorTheme* colorTheme READ colorTheme NOTIFY colorThemeChanged)
public:
    QnContext(QObject *parent = NULL);
    virtual ~QnContext();

    QnConnectionManager *connectionManager() const {
        return m_connectionManager;
    }

    QnColorTheme *colorTheme() const {
        return m_colorTheme;
    }

signals:
    /* Dummy signals to prevent non-NOTIFYable warnings */
    void connectionManagerChanged(QnConnectionManager *connectionManager);
    void colorThemeChanged(QnColorTheme *colorTheme);

private:
    QnConnectionManager *m_connectionManager;
    QnColorTheme *m_colorTheme;
};

Q_DECLARE_METATYPE(QnContext*)

#endif // CONTEXT_H
