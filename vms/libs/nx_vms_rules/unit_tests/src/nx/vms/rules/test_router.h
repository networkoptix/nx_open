// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/router.h>

namespace nx::vms::rules::test {

class TestRouter: public nx::vms::rules::Router
{
public:
    virtual void routeEvent(
        const EventData& eventData,
        const QSet<QnUuid>& triggeredRules,
        const QSet<QnUuid>& affectedResources) override
    {
    }
};

} // namespace nx::vms::rules::test
