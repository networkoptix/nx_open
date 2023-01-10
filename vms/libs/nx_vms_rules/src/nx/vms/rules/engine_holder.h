// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "engine.h"

namespace nx::vms::common { class SystemContext; }

class QnCommonMessageProcessor;

namespace nx::vms::rules {

class Plugin;

/** Helper class for engine storage and initialization. */
class NX_VMS_RULES_API EngineHolder: public QObject
{
public:
    EngineHolder(nx::vms::common::SystemContext* context, std::unique_ptr<Plugin> plugin);
    ~EngineHolder();

    Engine* engine() const;

    static void connectEngine(
        Engine* engine,
        const QnCommonMessageProcessor* processor,
        Qt::ConnectionType connectionType);

private:
    std::unique_ptr<Engine> m_engine;
    std::unique_ptr<Plugin> m_builtinPlugin;
};

} // namespace nx::vms::rules
