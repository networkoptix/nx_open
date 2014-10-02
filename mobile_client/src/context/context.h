#ifndef CONTEXT_H
#define CONTEXT_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <utils/common/instance_storage.h>

class QnConnectionManager;

class QnContext: public QObject, public QnInstanceStorage {
    Q_OBJECT
    typedef QObject base_type;

    Q_PROPERTY(QnConnectionManager* connectionManager READ connectionManager NOTIFY connectionManagerChanged)
public:
    QnContext(QObject *parent = NULL);
    virtual ~QnContext();

    QnConnectionManager *connectionManager() const {
        return m_connectionManager;
    }

signals:
    /* Dummy signals to prevent non-NOTIFYable warnings */
    void connectionManagerChanged(QnConnectionManager *connectionManager);

private:
    QnConnectionManager *m_connectionManager;
};

Q_DECLARE_METATYPE(QnContext*)

#endif // CONTEXT_H
