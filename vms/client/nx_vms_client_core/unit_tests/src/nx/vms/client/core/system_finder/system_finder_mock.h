// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gmock/gmock.h>

#include <nx/vms/client/core/system_finder/system_finder.h>

namespace nx::vms::client::core::test {

class SystemFinderMock: public AbstractSystemFinder
{
    using base_type = AbstractSystemFinder;

public:
    SystemFinderMock(QObject* parent = nullptr):
        base_type(parent)
    {}

    void addSystem(const SystemDescriptionPtr& system)
    {
        m_systems.insert(system->id(), system);
        emit systemDiscovered(system);
    }

    void removeSystem(const QString& id)
    {
        const auto it = m_systems.find(id);
        if (it == m_systems.end())
            return;

        const auto system = *it;
        m_systems.erase(it);

        emit systemLost(system->id(), system->localId());
    }

    virtual SystemDescriptionList systems() const override
    {
        return m_systems.values();
    }

    virtual SystemDescriptionPtr getSystem(const QString& id) const override
    {
        const auto it = m_systems.find(id);

        return it != m_systems.end() ? *it : SystemDescriptionPtr();
    }

private:
    using SystemMap = QMap<QString, SystemDescriptionPtr>;

    SystemMap m_systems;
};

using SystemFinderMockPtr = std::unique_ptr<SystemFinderMock>;

} // namespace nx::vms::client::core::test
