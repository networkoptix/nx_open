#ifndef INCOMPATIBLE_SERVER_WATCHER_H
#define INCOMPATIBLE_SERVER_WATCHER_H

#include <QtCore/QObject>
#include <core/resource/resource_fwd.h>
#include <utils/common/uuid.h>


struct QnModuleInformation;

class QnIncompatibleServerWatcher : public QObject {
    Q_OBJECT
public:
    explicit QnIncompatibleServerWatcher(QObject *parent = 0);
    ~QnIncompatibleServerWatcher();

private slots:
    void at_peerChanged(const QnModuleInformation &moduleInformation);
    void at_peerLost(const QnModuleInformation &moduleInformation);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

private:
    void removeResource(const QnUuid &id);

private:
    QHash<QnUuid, QnUuid> m_fakeUuidByServerUuid;
    QHash<QnUuid, QnUuid> m_serverUuidByFakeUuid;
};

#endif // INCOMPATIBLE_SERVER_WATCHER_H
