// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/plugin.h>
#include <nx/vms/rules/event_fields/state_field.h>

#include "test_field.h"
#include "test_event.h"
#include "test_action.h"

namespace nx::vms::rules::test {

class TestPlugin: public nx::vms::rules::Plugin
{
public:
    TestPlugin(Engine* engine)
    {
        initialize(engine);
    };

    virtual void registerFields() const override
    {
        registerEventField<StateField>();
        registerEventField<TestEventField>();
    }

    void registerEvents() const
    {
        registerEvent<TestEvent>();
    }

    void registerActions() const
    {
        registerAction<TestProlongedAction>();
    }
};

} // namespace nx::vms::rules::test
