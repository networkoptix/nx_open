// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct InternalState;
class ErrorHandler;
class Plugin;

class Engine: public AbstractEngine
{
public:
    Engine(nx::vms::api::analytics::EngineDescriptor engineDescriptor,
        QObject* parent = nullptr);

    virtual QString id() const override;

    virtual QString name() const override;

    virtual AbstractPlugin* plugin() const override;

    virtual nx::vms::api::analytics::EngineCapabilities capabilities() const override;

    virtual nx::vms::api::analytics::EngineDescriptor serialize() const override;

    void setPlugin(Plugin* plugin);

    void resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler);

private:
    nx::vms::api::analytics::EngineDescriptor m_descriptor;
    Plugin* m_plugin = nullptr;
};

} // namespace nx::analytics::taxonomy
