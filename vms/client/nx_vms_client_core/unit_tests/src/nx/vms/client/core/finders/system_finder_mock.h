// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gmock/gmock.h>

#include <finders/systems_finder.h>

namespace nx::vms::client::core::test {

class SystemFinderMock: public QnAbstractSystemsFinder
{
    using base_type = QnAbstractSystemsFinder;

public:
    SystemFinderMock(QObject* parent = nullptr):
        base_type(parent)
    {}

    void addSystem(const QnSystemDescriptionPtr& system)
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

    void clear()
    {
        for (const auto& system: m_systems)
            emit systemLost(system->id(), system->localId());
        m_systems.clear();
    }

    virtual SystemDescriptionList systems() const override
    {
        return m_systems.values();
    }

    virtual QnSystemDescriptionPtr getSystem(const QString& id) const override
    {
        const auto it = m_systems.find(id);

        return it != m_systems.end() ? *it : QnSystemDescriptionPtr();
    }

private:
    using SystemMap = QMap<QString, QnSystemDescriptionPtr>;

    SystemMap m_systems;
};

using SystemFinderMockPtr = std::unique_ptr<SystemFinderMock>;

} // namespace nx::vms::client::core::test
