#ifndef SERVER_INTERFACE_WATCHER_H
#define SERVER_INTERFACE_WATCHER_H

#include <QtCore/QObject>

class QnRouter;

class QnServerInterfaceWatcher : public QObject {
    Q_OBJECT
public:
    explicit QnServerInterfaceWatcher(QnRouter *router, QObject *parent = 0);

private slots:
    void at_connectionChanged(const QUuid &discovererId, const QUuid &peerId);
};

#endif // SERVER_INTERFACE_WATCHER_H
