// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

namespace nx::vms::rules::test {

class TestEvent: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.test")

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = "nx.event.test",
            .displayName = "Test event",
            .description = "Test description",
        };
    }
};

} // namespace nx::vms::rules::test
