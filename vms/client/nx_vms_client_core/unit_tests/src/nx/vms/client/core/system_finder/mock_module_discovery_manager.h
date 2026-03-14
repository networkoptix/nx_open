// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/discovery/abstract_manager.h>

namespace nx::vms::client::core::test {

/**
 * Mock for nx::vms::discovery::Manager that allows emitting signals for testing DirectSystemFinder.
 * Provides the same signal interface as the real Manager without network dependencies.
 */
class MockModuleDiscoveryManager: public nx::vms::discovery::AbstractManager
{
    Q_OBJECT

public:
    explicit MockModuleDiscoveryManager(QObject* parent = nullptr):
        nx::vms::discovery::AbstractManager(parent)
    {
    }

    std::list<nx::vms::discovery::ModuleEndpoint> getAll() const override { return m_modules; }

    void emitFound(const nx::vms::discovery::ModuleEndpoint& module)
    {
        m_modules.push_back(module);
        emit found(module);
    }

    void emitChanged(const nx::vms::discovery::ModuleEndpoint& module)
    {
        emit changed(module);
    }

    void emitLost(nx::Uuid id)
    {
        m_modules.remove_if([&id](const auto& m) { return m.id == id; });
        emit lost(id);
    }

private:
    std::list<nx::vms::discovery::ModuleEndpoint> m_modules;
};

} // namespace nx::vms::client::core::test
