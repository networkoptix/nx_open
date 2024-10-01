// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>

#include "integration.h"
#include "stream_parser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine(Integration* integration);
    virtual ~Engine() override;

protected:
    virtual std::string manifestString() const override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    void reportIssues(const Issues& issues) const;

private:
    Integration* m_integration = nullptr;
};

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
