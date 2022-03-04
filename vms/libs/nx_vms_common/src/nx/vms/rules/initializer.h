// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/plugin.h>

namespace nx::vms::rules {

class NX_VMS_COMMON_API Initializer: public Plugin
{

public:
    Initializer(Engine* engine);
    ~Initializer();

    virtual void registerEvents() const override;
    virtual void registerActions() const override;
    virtual void registerFields() const override;
};

} // namespace nx::vms::rules
