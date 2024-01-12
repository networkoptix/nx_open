// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <finders/abstract_systems_finder.h>
#include <network/system_description.h>
#include <nx/vms/discovery/manager.h>

class SearchAddressManager;

/* Performs local system auto discovery based on nx::vms::discovery::Manager. */
class QnDirectSystemsFinder : public QnAbstractSystemsFinder
{
    Q_OBJECT
    typedef QnAbstractSystemsFinder base_type;

public:
    QnDirectSystemsFinder(SearchAddressManager* searchAddressManager, QObject* parent = nullptr);

    SystemDescriptionList systems() const override;
    QnSystemDescriptionPtr getSystem(const QString &id) const override;

private:
    void updateServerData(const nx::vms::discovery::ModuleEndpoint& module);
    void removeServer(QnUuid id);

    typedef QHash<QString, QnSystemDescription::PointerType> SystemsHash;
    void updateServerInternal(
        SystemsHash::iterator systemIt, const nx::vms::discovery::ModuleEndpoint& module);

    void updatePrimaryAddress(const nx::vms::discovery::ModuleEndpoint& module);
    SystemsHash::iterator getSystemItByServer(const QnUuid &serverId);
    void removeSystem(SystemsHash::iterator it);

private:
    typedef QHash<QnUuid, QString> ServerToSystemHash;

    SystemsHash m_systems;
    ServerToSystemHash m_serverToSystem;

    SearchAddressManager* m_searchAddressManager;
};
