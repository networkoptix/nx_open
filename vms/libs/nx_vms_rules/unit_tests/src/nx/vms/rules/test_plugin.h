// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/plugin.h>

#include "test_action.h"
#include "test_event.h"
#include "test_field.h"

namespace nx::vms::rules::test {

class TestPlugin: public nx::vms::rules::Plugin
{
public:
    TestPlugin(Engine* engine = nullptr)
    {
        if (engine)
            initialize(engine);
    };

    virtual void registerFields() const override
    {
        registerEventField<StateField>();
        registerEventField<TestEventField>();

        registerActionField<TestActionField>();
        registerActionField<TargetDeviceField>();
        registerActionField<OptionalTimeField>();
    }

    virtual void registerEvents() const override
    {
        registerEvent<SimpleEvent>();
        registerEvent<TestEvent>();
    }

    virtual void registerActions() const override
    {
        registerAction<TestAction>();
        registerAction<TestProlongedAction>();
        registerAction<TestActionWithInterval>();
    }
};

} // namespace nx::vms::rules::test
