// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/rules/plugin.h>

namespace nx::vms::rules {

class NX_VMS_RULES_API Initializer: public nx::vms::common::SystemContextAware, public Plugin
{

public:
    Initializer(nx::vms::common::SystemContext* context);
    ~Initializer();

    virtual void registerEvents() const override;
    virtual void registerActions() const override;
    virtual void registerFields() const override;
    virtual void registerFieldValidators() const override;
};

} // namespace nx::vms::rules
