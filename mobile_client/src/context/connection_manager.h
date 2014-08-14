#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QtCore/QObject>

#include <context/context_aware.h>

class QnConnectionManager : public QObject, public QnContextAware {
    Q_OBJECT
public:
    explicit QnConnectionManager(QObject *parent = 0);

signals:
    void connected();
    void disconnected();

public slots:
    void connect(const QUrl &url);
    void disconnect(bool force);
};

#endif // CONNECTION_MANAGER_H
