// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_executor.h>
#include <nx/vms/rules/event_connector.h>

namespace nx::vms::rules::test {

class TestActionExecutor: public ActionExecutor
{
public:
    void execute(const ActionPtr& action)
    {
        actions.push_back(action);
    }

public:
    std::vector<ActionPtr> actions;
};

class TestEventConnector: public EventConnector
{
public:
    void process(const EventPtr& e)
    {
        emit event(e);
    }
};

} // namespace nx::vms::rules::test
