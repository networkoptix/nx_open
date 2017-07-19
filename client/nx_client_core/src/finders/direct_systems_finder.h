
#pragma once

#include <finders/abstract_systems_finder.h>
#include <network/system_description.h>
#include <nx/network/socket_common.h>
#include <nx/vms/discovery/manager.h>

struct QnModuleInformation;

class QnDirectSystemsFinder : public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    QnDirectSystemsFinder(QObject *parent = nullptr);

public: // overrides
    SystemDescriptionList systems() const override;

    QnSystemDescriptionPtr getSystem(const QString &id) const override;

private:
    void updateServerData(nx::vms::discovery::ModuleEndpoint module);
    void removeServer(QnUuid id);

    typedef QHash<QString, QnSystemDescription::PointerType> SystemsHash;
    void updateServerInternal(
        const SystemsHash::iterator systemIt, nx::vms::discovery::ModuleEndpoint module);

    void updatePrimaryAddress(nx::vms::discovery::ModuleEndpoint module);
    SystemsHash::iterator getSystemItByServer(const QnUuid &serverId);
    void removeSystem(const SystemsHash::iterator& it);

private:
    typedef QHash<QnUuid, QString> ServerToSystemHash;

    SystemsHash m_systems;
    ServerToSystemHash m_serverToSystem;
};
