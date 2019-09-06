#pragma once

#include <memory>

namespace nx::vms::server::analytics::wrappers {

class DeviceAgent;
class Engine;
class Plugin;

using DeviceAgentPtr = std::shared_ptr<DeviceAgent>;
using EnginePtr = std::shared_ptr<Engine>;
using PluginPtr = std::shared_ptr<Plugin>;

} // namespace nx::vms::server::analytics::wrappers
