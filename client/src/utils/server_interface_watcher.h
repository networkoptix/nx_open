#ifndef SERVER_INTERFACE_WATCHER_H
#define SERVER_INTERFACE_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/uuid.h>


class QnRouter;

class QnServerInterfaceWatcher : public QObject {
    Q_OBJECT
public:
    explicit QnServerInterfaceWatcher(QnRouter *router, QObject *parent = 0);

private slots:
    void at_connectionChanged(const QnUuid &discovererId, const QnUuid &peerId);
    void at_resourcePool_statusChanged(const QnResourcePtr &resource);

private:
    void updatePriaryInterface(const QnMediaServerResourcePtr &server);
};

#endif // SERVER_INTERFACE_WATCHER_H
