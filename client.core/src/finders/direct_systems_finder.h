
#pragma once

#include <finders/abstract_systems_finder.h>

struct QnModuleInformation;

class QnDirectSystemsFinder : public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    QnDirectSystemsFinder(QObject *parent = nullptr);

    virtual ~QnDirectSystemsFinder();

public: // overrides
    SystemDescriptionList systems() const override;

private:
    void addServer(const QnModuleInformation &moduleInformation);

    void removeServer(const QnModuleInformation &moduleInformation);

    typedef QHash<QString, QnSystemDescriptionPtr> SystemsHash;
    void updateServer(const SystemsHash::iterator systemIt
        , const QnModuleInformation &moduleInformation);

    void updatePrimaryAddress(const QnModuleInformation &moduleInformation
        , const SocketAddress &address);

    SystemsHash::iterator getSystemItByServer(const QnUuid &serverId);

private:
    typedef QHash<QnUuid, SystemsHash::iterator> ServerToSystemHash;

    SystemsHash m_systems;
    ServerToSystemHash m_serverToSystem;
};