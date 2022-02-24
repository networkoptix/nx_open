// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/plugin.h>

#include "test_field.h"

namespace nx::vms::rules::test {

class TestPlugin: public nx::vms::rules::Plugin
{
public:
    TestPlugin(Engine* engine): Plugin(engine) {};

    virtual void registerFields() const override
    {
        registerEventField<TestEventField>();
    }
};

} // namespace nx::vms::rules::test
