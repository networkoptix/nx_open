
#pragma once

#include <finders/abstract_systems_finder.h>
#include <network/system_description.h>
#include <nx/network/socket_common.h>

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

    QnSystemDescriptionPtr getSystem(const QString &id) const override;

private:
    void addServer(const QnModuleInformation &moduleInformation);

    void removeServer(const QnModuleInformation &moduleInformation);

    typedef QHash<QString, QnSystemDescription::PointerType> SystemsHash;
    void updateServer(const SystemsHash::iterator systemIt
        , const QnModuleInformation &moduleInformation);

    void updatePrimaryAddress(const QnModuleInformation &moduleInformation
        , const SocketAddress &address);

    SystemsHash::iterator getSystemItByServer(const QnUuid &serverId);

private:
    typedef QHash<QnUuid, QString> ServerToSystemHash;

    SystemsHash m_systems;
    ServerToSystemHash m_serverToSystem;
};