// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../abstract_system_finder.h"

namespace nx::vms::client::core {

/**
 * This class is a common base for ScopeLocalSystemFinder and RecentLocalSystemFinder.
 * It contains logic of getting diff between two sets of systems and sending corresponding signals.
 */
class LocalSystemFinder: public AbstractSystemFinder
{
    Q_OBJECT
    using base_type = AbstractSystemFinder;

public:
    LocalSystemFinder(QObject* parent = nullptr);
    virtual ~LocalSystemFinder() = default;

    virtual SystemDescriptionList systems() const override;
    virtual SystemDescriptionPtr getSystem(const QString& id) const override;

protected:
    virtual void updateSystems() = 0;

    typedef QHash<QString, SystemDescriptionPtr> SystemsHash;
    void setSystems(const SystemsHash& newSystems);

private:
    void removeSystem(const QString& id);

private:
    SystemsHash m_systems;
};

} // namespace nx::vms::client::core
