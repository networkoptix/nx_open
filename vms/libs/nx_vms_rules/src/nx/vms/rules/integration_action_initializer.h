// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/plugin.h>

namespace nx::vms::rules {

static const QString kIntegrationActionTypePrefix = "integration.";

class NX_VMS_RULES_API IntegrationActionInitializer:
    public nx::vms::common::SystemContextAware, public Plugin
{

public:
    IntegrationActionInitializer(nx::vms::common::SystemContext* context);
    ~IntegrationActionInitializer();

    void registerActions() const override;
    void registerFields() const override;
    void deinitialize() override;

private:
    mutable std::set<QString> m_registeredActionTypes;
};

} // namespace nx::vms::rules
