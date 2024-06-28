// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/discovery/manager.h>

#include "../abstract_system_finder.h"
#include "local_system_description.h"

namespace nx::vms::client::core {

class SearchAddressManager;

/* Performs local system auto discovery based on nx::vms::discovery::Manager. */
class DirectSystemFinder: public AbstractSystemFinder
{
    Q_OBJECT
    typedef AbstractSystemFinder base_type;

public:
    DirectSystemFinder(SearchAddressManager* searchAddressManager, QObject* parent = nullptr);

    SystemDescriptionList systems() const override;
    SystemDescriptionPtr getSystem(const QString &id) const override;

private:
    void updateServerData(const nx::vms::discovery::ModuleEndpoint& module);
    void removeServer(nx::Uuid id);

    using SystemsHash = QHash<QString, LocalSystemDescriptionPtr>;
    void updateServerInternal(
        SystemsHash::iterator systemIt, const nx::vms::discovery::ModuleEndpoint& module);

    void updatePrimaryAddress(const nx::vms::discovery::ModuleEndpoint& module);
    SystemsHash::iterator getSystemItByServer(const nx::Uuid &serverId);
    void removeSystem(SystemsHash::iterator it);

private:
    typedef QHash<nx::Uuid, QString> ServerToSystemHash;

    SystemsHash m_systems;
    ServerToSystemHash m_serverToSystem;

    SearchAddressManager* m_searchAddressManager;
};

} // namespace nx::vms::client::core
