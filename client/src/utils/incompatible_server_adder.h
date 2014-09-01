#ifndef INCOMPATIBLE_SERVER_ADDER_H
#define INCOMPATIBLE_SERVER_ADDER_H

#include <QtCore/QObject>
#include <core/resource/resource_fwd.h>

struct QnModuleInformation;

class QnIncompatibleServerAdder : public QObject {
    Q_OBJECT
public:
    explicit QnIncompatibleServerAdder(QObject *parent = 0);

private slots:
    void at_peerChanged(const QnModuleInformation &moduleInformation);
    void at_peerLost(const QnModuleInformation &moduleInformation);
    void at_reourcePool_resourceChanged(const QnResourcePtr &resource);

private:
    void removeResource(const QUuid &id);

private:
    QHash<QUuid, QUuid> m_serverUuidByModuleUuid;
};

#endif // INCOMPATIBLE_SERVER_ADDER_H
