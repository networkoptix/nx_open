// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <iostream>

#include <gtest/gtest.h>

#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/analytics/taxonomy/test_resource_support_proxy.h>
#include <nx/vms/common/test_support/analytics/taxonomy/utils.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/analytics_attributes_field.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_type_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/text_lookup_field.h>
#include <nx/vms/rules/events/analytics_event.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/rule_cache.h>
#include <nx/vms/rules/utils/api.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>

#include "test_action.h"
#include "test_event.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

const QString kInstantEventType = utils::type<TestEventInstant>();
const QString kProlongedEventType = utils::type<TestEventProlonged>();
const QString kAnalyticsEventType = utils::type<AnalyticsEvent>();

const QString kAnalyticsEventTypeId = "test.eventType1";

constexpr auto kTaxonomyData = R"json(
{
    "descriptors": {
        "eventTypeDescriptors": {
            "test.eventType1": {
                "id": "test.eventType1",
                "name": "Test analytics event type"
            }
        }
    },
    "tests": {}
}
)json";

} // namespace

class RuleCacheTest: public EngineBasedTest, public TestPlugin
{
public:
    RuleCacheTest(): TestPlugin(engine.get()) {};

    RuleCache* cache() const { return engine->ruleCache(); }

    size_t indexedCount(const QString& eventType) const
    {
        return cache()->byEventType(eventType).size();
    }

    const RuleCache::Entry* entry(const QString& eventType, nx::Uuid ruleId) const
    {
        for (const auto& entry: cache()->byEventType(eventType))
        {
            if (entry.rule->id() == ruleId)
                return &entry;
        }

        return nullptr;
    }

    /** Adds the rule to the engine and returns its id. */
    nx::Uuid add(const Rule* rule)
    {
        engine->updateRule(serialize(rule));
        return rule->id();
    }
};

TEST_F(RuleCacheTest, compatibleRuleIsIndexedByItsEventType)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    const auto ruleId = add(rule.get());

    ASSERT_EQ(indexedCount(kInstantEventType), 1U);
    EXPECT_TRUE(cache()->isCompatible(ruleId));

    const auto entry = this->entry(kInstantEventType, ruleId);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->filters.size(), 1U);
    EXPECT_EQ(entry->filters.front()->eventType(), kInstantEventType);

    // The rule must not be indexed by an unrelated event type.
    EXPECT_TRUE(cache()->byEventType(kProlongedEventType).empty());
}

TEST_F(RuleCacheTest, updatedRuleIsIndexedOnce)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    const auto ruleId = add(rule.get());
    add(rule.get());

    EXPECT_EQ(indexedCount(kInstantEventType), 1U);
    EXPECT_TRUE(cache()->isCompatible(ruleId));
}

TEST_F(RuleCacheTest, removedRuleIsNotIndexed)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    const auto ruleId = add(rule.get());

    engine->removeRule(ruleId);

    EXPECT_TRUE(cache()->byEventType(kInstantEventType).empty());
    EXPECT_FALSE(cache()->isCompatible(ruleId));
}

TEST_F(RuleCacheTest, removingRuleKeepsOtherRulesOfTheSameEventType)
{
    // The second rule gets an extra filter of the same event type, so the two entries of the
    // bucket are distinguishable and mixing them up during the removal is visible.
    auto firstRule = makeRule<TestEventInstant, TestAction>();
    const auto firstId = add(firstRule.get());

    auto secondRule = makeRule<TestEventInstant, TestAction>();
    secondRule->addEventFilter(engine->buildEventFilter(kInstantEventType));
    const auto secondId = add(secondRule.get());

    ASSERT_EQ(indexedCount(kInstantEventType), 2U);

    engine->removeRule(firstId);

    // The bucket must survive with the remaining rule and its own filters untouched.
    ASSERT_EQ(indexedCount(kInstantEventType), 1U);
    EXPECT_FALSE(entry(kInstantEventType, firstId));
    EXPECT_FALSE(cache()->isCompatible(firstId));

    const auto remaining = entry(kInstantEventType, secondId);
    ASSERT_TRUE(remaining);
    EXPECT_EQ(remaining->filters.size(), 2U);
    EXPECT_TRUE(cache()->isCompatible(secondId));

    // The bucket is dropped only when the last rule of the event type is gone.
    engine->removeRule(secondId);
    EXPECT_TRUE(cache()->byEventType(kInstantEventType).empty());
}

TEST_F(RuleCacheTest, updatingRuleKeepsOtherRulesOfTheSameEventType)
{
    auto firstRule = makeRule<TestEventInstant, TestAction>();
    const auto firstId = add(firstRule.get());

    auto secondRule = makeRule<TestEventInstant, TestAction>();
    secondRule->addEventFilter(engine->buildEventFilter(kInstantEventType));
    const auto secondId = add(secondRule.get());

    // An update removes the rule from the index and adds it again, so the neighbour entries of
    // the bucket must not be affected.
    add(firstRule.get());

    ASSERT_EQ(indexedCount(kInstantEventType), 2U);

    const auto first = entry(kInstantEventType, firstId);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->filters.size(), 1U);

    const auto second = entry(kInstantEventType, secondId);
    ASSERT_TRUE(second);
    EXPECT_EQ(second->filters.size(), 2U);
}

TEST_F(RuleCacheTest, disabledRuleIsNotIndexed)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    rule->setEnabled(false);
    const auto ruleId = add(rule.get());

    EXPECT_TRUE(cache()->byEventType(kInstantEventType).empty());

    // Being disabled does not make the rule incompatible, and the engine still keeps it.
    EXPECT_TRUE(cache()->isCompatible(ruleId));
    EXPECT_TRUE(engine->rule(ruleId));
}

TEST_F(RuleCacheTest, incompatibleRuleIsNotIndexed)
{
    // An instant event is not able to trigger a purely prolonged action.
    auto rule = makeRule<TestEventInstant, TestProlongedOnlyAction>();
    const auto ruleId = add(rule.get());

    EXPECT_TRUE(cache()->byEventType(kInstantEventType).empty());
    EXPECT_FALSE(cache()->isCompatible(ruleId));
    EXPECT_TRUE(engine->rule(ruleId));
}

TEST_F(RuleCacheTest, incompleteRuleIsNotIndexedButIsKeptByEngine)
{
    const api::Rule ruleData{{nx::Uuid::createUuid()}};
    engine->updateRule(ruleData);

    EXPECT_TRUE(cache()->byEventType(kInstantEventType).empty());
    EXPECT_FALSE(cache()->isCompatible(ruleData.id));
    EXPECT_TRUE(engine->rule(ruleData.id));
}

TEST_F(RuleCacheTest, ruleWithSeveralEventTypesIsIndexedByEachOfThem)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    rule->addEventFilter(engine->buildEventFilter(kProlongedEventType));
    const auto ruleId = add(rule.get());

    ASSERT_EQ(indexedCount(kInstantEventType), 1U);
    ASSERT_EQ(indexedCount(kProlongedEventType), 1U);

    // Every entry contains the filters of the event type it is indexed by only.
    const auto instantEntry = entry(kInstantEventType, ruleId);
    ASSERT_TRUE(instantEntry);
    ASSERT_EQ(instantEntry->filters.size(), 1U);
    EXPECT_EQ(instantEntry->filters.front()->eventType(), kInstantEventType);

    const auto prolongedEntry = entry(kProlongedEventType, ruleId);
    ASSERT_TRUE(prolongedEntry);
    ASSERT_EQ(prolongedEntry->filters.size(), 1U);
    EXPECT_EQ(prolongedEntry->filters.front()->eventType(), kProlongedEventType);

    engine->removeRule(ruleId);

    EXPECT_TRUE(cache()->byEventType(kInstantEventType).empty());
    EXPECT_TRUE(cache()->byEventType(kProlongedEventType).empty());
}

TEST_F(RuleCacheTest, filtersOfTheSameEventTypeAreKeptInSingleEntry)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    rule->addEventFilter(engine->buildEventFilter(kInstantEventType));
    const auto ruleId = add(rule.get());

    ASSERT_EQ(indexedCount(kInstantEventType), 1U);

    const auto entry = this->entry(kInstantEventType, ruleId);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->filters.size(), 2U);
}

TEST_F(RuleCacheTest, rulesResetRebuildsIndex)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    const auto ruleData = serialize(rule.get());

    engine->resetRules({ruleData});
    ASSERT_EQ(indexedCount(kInstantEventType), 1U);
    EXPECT_TRUE(cache()->isCompatible(ruleData.id));

    engine->resetRules({});
    EXPECT_TRUE(cache()->byEventType(kInstantEventType).empty());
    EXPECT_FALSE(cache()->isCompatible(ruleData.id));
}

TEST_F(RuleCacheTest, taxonomyChangeDoesNotAffectNonAnalyticsRules)
{
    auto rule = makeRule<TestEventInstant, TestAction>();
    const auto ruleId = add(rule.get());

    engine->onTaxonomyChanged();

    ASSERT_EQ(indexedCount(kInstantEventType), 1U);
    EXPECT_TRUE(entry(kInstantEventType, ruleId));
    EXPECT_TRUE(cache()->isCompatible(ruleId));
}

/**
 * Rule compatibility of the analytics events depends on the taxonomy state, so it must be
 * recalculated once the taxonomy is changed. The realistic startup order is the rules arriving
 * from the database before the analytics descriptors, so the initial state is the empty taxonomy.
 */
class RuleCacheTaxonomyTest: public RuleCacheTest
{
public:
    virtual void SetUp() override
    {
        RuleCacheTest::SetUp();

        ASSERT_TRUE(registerEventField<AnalyticsEventTypeField>(systemContext()));
        ASSERT_TRUE(registerEventField<TextLookupField>(systemContext()));
        ASSERT_TRUE(registerEventField<AnalyticsAttributesField>());
    }

    void loadTaxonomy(bool stateDependent)
    {
        nx::analytics::taxonomy::TestData testData;
        ASSERT_TRUE(nx::analytics::taxonomy::makeDescriptorsTestData(kTaxonomyData, &testData));

        // The event duration type, and hence the rule compatibility, is derived from the flag.
        testData.descriptors.eventTypeDescriptors[kAnalyticsEventTypeId.toStdString()].flags =
            stateDependent
            ? nx::vms::api::analytics::EventTypeFlags(
                  nx::vms::api::analytics::EventTypeFlag::stateDependent)
            : nx::vms::api::analytics::EventTypeFlags();

        const auto compiled = nx::analytics::taxonomy::StateCompiler::compile(testData.descriptors,
            std::make_unique<nx::analytics::taxonomy::TestResourceSupportProxy>());
        ASSERT_TRUE(compiled.errors.empty());

        systemContext()->analyticsDescriptorContainer()->updateDescriptorsForTests(
            compiled.state->serialize());
    }

    /**
     * An analytics event rule with the event type set. A prolonged only action is used, so the
     * rule is compatible only while the taxonomy reports the event type as state dependent.
     */
    std::unique_ptr<Rule> makeAnalyticsRule()
    {
        auto rule = makeRule<AnalyticsEvent, TestProlongedOnlyAction>();

        auto field = rule->eventFilters().front()->fieldByName<AnalyticsEventTypeField>(
            utils::kEventTypeIdFieldName);
        if (!field)
            return {};

        field->setTypeId(kAnalyticsEventTypeId);

        return rule;
    }
};

TEST_F(RuleCacheTaxonomyTest, ruleIsIndexedOnceTaxonomyMakesItCompatible)
{
    auto rule = makeAnalyticsRule();
    ASSERT_TRUE(rule);
    const auto ruleId = add(rule.get());

    // The taxonomy is empty, so the event is treated as instant, and an instant event is not able
    // to trigger a prolonged only action.
    EXPECT_FALSE(cache()->isCompatible(ruleId));
    EXPECT_TRUE(cache()->byEventType(kAnalyticsEventType).empty());

    loadTaxonomy(/*stateDependent*/ true);
    engine->onTaxonomyChanged();

    // The event became prolonged, so the rule is able to produce actions and must be indexed.
    EXPECT_TRUE(cache()->isCompatible(ruleId));
    ASSERT_EQ(indexedCount(kAnalyticsEventType), 1U);

    const auto indexed = entry(kAnalyticsEventType, ruleId);
    ASSERT_TRUE(indexed);
    EXPECT_EQ(indexed->filters.size(), 1U);
}

TEST_F(RuleCacheTaxonomyTest, ruleIsRemovedFromIndexOnceTaxonomyMakesItIncompatible)
{
    auto rule = makeAnalyticsRule();
    ASSERT_TRUE(rule);
    const auto ruleId = add(rule.get());

    loadTaxonomy(/*stateDependent*/ true);
    engine->onTaxonomyChanged();
    ASSERT_EQ(indexedCount(kAnalyticsEventType), 1U);

    loadTaxonomy(/*stateDependent*/ false);
    engine->onTaxonomyChanged();

    EXPECT_FALSE(cache()->isCompatible(ruleId));
    EXPECT_TRUE(cache()->byEventType(kAnalyticsEventType).empty());
}

} // namespace nx::vms::rules::test
