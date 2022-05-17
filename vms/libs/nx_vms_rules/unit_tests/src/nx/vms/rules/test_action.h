// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_action.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

class TestAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.testAction")
};

class TestProlongedAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.test.prolonged")

public:

    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestProlongedAction>(),
            .displayName = "Test prolonged event",
            .flags = ItemFlag::prolonged,
        };
    }
};

} // namespace nx::vms::rules::test
