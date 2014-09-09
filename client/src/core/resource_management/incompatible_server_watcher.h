#ifndef INCOMPATIBLE_SERVER_WATCHER_H
#define INCOMPATIBLE_SERVER_WATCHER_H

#include <QtCore/QObject>
#include <core/resource/resource_fwd.h>

struct QnModuleInformation;

class QnIncompatibleServerWatcher : public QObject {
    Q_OBJECT
public:
    explicit QnIncompatibleServerWatcher(QObject *parent = 0);

private slots:
    void at_peerChanged(const QnModuleInformation &moduleInformation);
    void at_peerLost(const QnModuleInformation &moduleInformation);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

private:
    void removeResource(const QUuid &id);

private:
    QHash<QUuid, QUuid> m_fakeUuidByServerUuid;
    QHash<QUuid, QUuid> m_serverUuidByFakeUuid;
};

#endif // INCOMPATIBLE_SERVER_WATCHER_H
