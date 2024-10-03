// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/event_filter.h>

#include "test_action.h"
#include "test_event.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

class RuleTest: public EngineBasedTest, public TestPlugin
{
public:
    RuleTest(): TestPlugin{engine.get()}
    {
    }
};

TEST_F(RuleTest, ruleWithIncompatibleEventAndActionIsInvalid)
{
    auto rule = std::make_unique<Rule>(nx::Uuid::createUuid(), m_engine);

    rule->addEventFilter(m_engine->buildEventFilter(utils::type<TestEventInstant>()));
    rule->addActionBuilder(m_engine->buildActionBuilder(utils::type<TestProlongedOnlyAction>()));

    ASSERT_TRUE(rule->isCompleted());
    ASSERT_FALSE(rule->isCompatible());
    ASSERT_EQ(rule->validity(systemContext()), QValidator::State::Invalid);
}

} // namespace nx::vms::rules::test
