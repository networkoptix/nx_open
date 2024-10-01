// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_integration.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class Integration: public AbstractIntegration
{
public:
    Integration(
        nx::vms::api::analytics::PluginDescriptor pluginDescriptor,
        QObject* parent = nullptr);

    virtual QString id() const override;

    virtual QString name() const override;

    virtual nx::vms::api::analytics::PluginDescriptor serialize() const override;

private:
    nx::vms::api::analytics::PluginDescriptor m_descriptor;
};

} // namespace nx::analytics::taxonomy
