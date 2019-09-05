#include <gtest/gtest.h>

#include <nx/vms/server/analytics/rule_holder.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/resource/device_mock.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/event/events/events.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::event;

static const QString kEventTypeId = "someEventTypeId";
static const QString kOtherEventTypeId = "someOtherEventTypeId";
static constexpr int kResourcesCount = 5;

static RulePtr makeRule(const QnResourcePtr& resource)
{
    RulePtr rule(new Rule());
    rule->setId(QnUuid::createUuid());
    rule->setEventResources({resource->getId()});

    return rule;
}

static RulePtr makeAnalyticsRule(const QnResourcePtr& resource, const QString& eventTypeId)
{
    RulePtr rule = makeRule(resource);
    rule->setEventType(EventType::analyticsSdkEvent);

    EventParameters eventParameters;
    eventParameters.setAnalyticsEventTypeId(eventTypeId);
    rule->setEventParams(eventParameters);

    return rule;
}

static RulePtr makeRegularRule(const QnResourcePtr& resource)
{
    RulePtr rule = makeRule(resource);
    rule->setEventType(EventType::userDefinedEvent);

    return rule;
}

class RuleHolderTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_resources.clear();
        m_affectedResources.clear();

        m_commonModule = std::make_unique<QnCommonModule>(
            /*clientMode*/ false,
            nx::core::access::Mode::direct);

        m_commonModule->setModuleGUID(QnUuid::createUuid());
        m_ruleHolder = std::make_unique<RuleHolder>(m_commonModule.get());

    }

protected:
    void givenSomeResources()
    {
        for (auto i = 0; i < kResourcesCount; ++i)
        {
            QnResourcePtr resource(
                new nx::core::resource::DeviceMock(m_commonModule.get()));

            resource->setIdUnsafe(QnUuid::createUuid());
            m_commonModule->resourcePool()->addResource(resource);
            m_resources.push_back(resource);
        }
    }

    void givenSomeResourcesAndAnalyticsRuleForOneOfThem()
    {
        givenSomeResources();
        addAnalyticsRuleForResource(m_resources[0], kEventTypeId);
    }

    void givenSomeResourcesAndDisabledAnalyticsRuleForOneOfThem()
    {
        givenSomeResourcesAndAnalyticsRuleForOneOfThem();
        const RulePtr rule = firstResourceRule(m_resources[0]);
        rule->setDisabled(true);
        updateRule(rule);
    }

    void givenAnalyticsRuleWithMultipleResources()
    {
        givenSomeResources();
        const RulePtr rule = makeAnalyticsRule(m_resources[0], kEventTypeId);

        QVector<QnUuid> resourceIds;
        for (const auto& resource : m_resources)
            resourceIds.push_back(resource->getId());

        rule->setEventResources(resourceIds);
        addRule(rule);
    }

    void givenAnalyticsRuleWithEmptyResourceList()
    {
        givenSomeResources();
        const RulePtr rule = makeAnalyticsRule(m_resources[0], kEventTypeId);
        rule->setEventResources({});
        addRule(rule);
    }

    void givenSomeResourcesAndNonAnalyticsRuleForOneOfThem()
    {
        givenSomeResources();
        const RulePtr rule = makeRegularRule(m_resources[0]);
        addRule(rule);
    }

    //---------------------------------------------------------------------------------------------

    void afterAddingAnalyticsRuleForResource()
    {
        m_affectedResources = addAnalyticsRuleForResource(m_resources[0], kEventTypeId);
    }

    void afterRemovingAnalyticsRule()
    {
        const auto rule = firstResourceRule(m_resources[0]);
        m_commonModule->eventRuleManager()->removeRule(rule->id());
        m_affectedResources = m_ruleHolder->removeRule(rule->id());
    }

    void afterChangingAnalyticsRuleEventType()
    {
        const auto rule = firstResourceRule(m_resources[0]);
        auto eventParameters = rule->eventParams();
        eventParameters.setAnalyticsEventTypeId(kOtherEventTypeId);
        rule->setEventParams(std::move(eventParameters));

        m_affectedResources = updateRule(rule);
    }

    void afterRuleDisabling()
    {
        const RulePtr rule = firstResourceRule(m_resources[0]);
        rule->setDisabled(true);
        m_affectedResources = updateRule(rule);
    }

    void afterRuleEnabling()
    {
        const RulePtr rule = firstResourceRule(m_resources[0]);
        rule->setDisabled(false);
        m_affectedResources = updateRule(rule);
    }

    void afterRemovingAllResourcesFromRule()
    {
        const auto rule = firstResourceRule(m_resources[0]);
        rule->setEventResources({});
        m_affectedResources = updateRule(rule);
    }

    void afterAddingResourceToRuleResourceList()
    {
        const RulePtr rule = firstNoResourceRule();
        rule->setEventResources({ m_resources[0]->getId() });
        m_affectedResources = updateRule(rule);
    }

    void afterTurningToAnalyticsRule()
    {
        const RulePtr rule = firstResourceRule(m_resources[0]);
        rule->setEventType(EventType::analyticsSdkEvent);

        auto eventParameters = rule->eventParams();
        eventParameters.setAnalyticsEventTypeId(kEventTypeId);
        rule->setEventParams(eventParameters);
        m_affectedResources = updateRule(rule);
    }

    void afterTurningToNonAnalyticsRule()
    {
        const RulePtr rule = firstResourceRule(m_resources[0]);
        rule->setEventType(EventType::userDefinedEvent);
        m_affectedResources = updateRule(rule);
    }

    //---------------------------------------------------------------------------------------------

    void makeSureResourceHasBeenAffected()
    {
        ASSERT_TRUE(m_affectedResources.contains(m_resources[0]->getId()));
    }

    void makeSureEventTypeIsWathcedForResource()
    {
        const auto watchedEvents = m_ruleHolder->watchedEvents(m_resources[0]->getId());
        ASSERT_TRUE(watchedEvents.contains(kEventTypeId));
    }

    void makeSureWatchedEventTypeListIsEmptyForResource()
    {
        ASSERT_TRUE(m_ruleHolder->watchedEvents(m_resources[0]->getId()).empty());
    }

    void makeSureWatchedEventTypeListForResourceContainsNewEventType()
    {
        ASSERT_TRUE(
            m_ruleHolder->watchedEvents(m_resources[0]->getId()).contains(kOtherEventTypeId));
    }

    void makeSureWatchedEventTypeListForResourceDoesNotContainOldEventType()
    {
        ASSERT_FALSE(
            m_ruleHolder->watchedEvents(m_resources[0]->getId()).contains(kEventTypeId));
    }

    void makeSureAllResourcesAreAffected()
    {
        for (const auto& resource : m_resources)
        {
            ASSERT_TRUE(m_affectedResources.contains(resource->getId()));
        }
    }

    void makeSureWatchedEventTypeListIsEmptyForAllResources()
    {
        for (const auto& resource : m_resources)
        {
            ASSERT_TRUE(m_ruleHolder->watchedEvents(resource->getId()).empty());
        }
    }

private:
    std::set<RulePtr> noResourceRules() const
    {
        return m_rulesByResource[QnUuid()];
    }

    RulePtr firstNoResourceRule() const
    {
        const auto rules = noResourceRules();
        return *rules.begin();
    }

    std::set<RulePtr> resourceRules(const QnResourcePtr& resource) const
    {
        return m_rulesByResource[resource->getId()];
    }

    RulePtr firstResourceRule(const QnResourcePtr& resource) const
    {
        const auto rules = resourceRules(resource);
        return *rules.begin();
    }

    QSet<QnUuid> addRule(const RulePtr& rule)
    {
        m_commonModule->eventRuleManager()->addOrUpdateRule(rule);
        auto affectedResources = m_ruleHolder->addRule(rule);

        const auto eventResources = rule->eventResources().toList().toSet();
        if (eventResources.isEmpty())
        {
            m_rulesByResource[QnUuid()].insert(rule);
        }
        else
        {
            for (const auto& resource : m_resources)
            {
                const auto resourceId = resource->getId();
                if (eventResources.contains(resourceId))
                    m_rulesByResource[resourceId].insert(rule);
            }
        }

        return affectedResources;
    }

    QSet<QnUuid> updateRule(const RulePtr& rule)
    {
        m_commonModule->eventRuleManager()->addOrUpdateRule(rule);
        auto affectedResources = m_ruleHolder->updateRule(rule);

        return affectedResources;
    }

    QSet<QnUuid> addAnalyticsRuleForResource(
        const QnResourcePtr& resource, const QString& eventTypeId)
    {
        const auto rule = makeAnalyticsRule(resource, eventTypeId);
        return addRule(rule);
    }

private:
    std::unique_ptr<QnCommonModule> m_commonModule;
    std::unique_ptr<RuleHolder> m_ruleHolder;

    mutable std::map<QnUuid, std::set<RulePtr>> m_rulesByResource;
    QnResourceList m_resources;

    QSet<QnUuid> m_affectedResources;
};

TEST_F(RuleHolderTest, addAnalyticsRule)
{
    givenSomeResources();
    afterAddingAnalyticsRuleForResource();
    makeSureResourceHasBeenAffected();
    makeSureEventTypeIsWathcedForResource();
}

TEST_F(RuleHolderTest, removeAnalyticsRule)
{
    givenSomeResourcesAndAnalyticsRuleForOneOfThem();
    afterRemovingAnalyticsRule();
    makeSureResourceHasBeenAffected();
    makeSureWatchedEventTypeListIsEmptyForResource();
}

TEST_F(RuleHolderTest, changeAnalyticsRule)
{
    givenSomeResourcesAndAnalyticsRuleForOneOfThem();
    afterChangingAnalyticsRuleEventType();
    makeSureResourceHasBeenAffected();
    makeSureWatchedEventTypeListForResourceContainsNewEventType();
    makeSureWatchedEventTypeListForResourceDoesNotContainOldEventType();
}

TEST_F(RuleHolderTest, disableAnalyticsRule)
{
    givenSomeResourcesAndAnalyticsRuleForOneOfThem();
    afterRuleDisabling();
    makeSureResourceHasBeenAffected();
    makeSureWatchedEventTypeListIsEmptyForResource();
}

TEST_F(RuleHolderTest, enableAnalyticsRule)
{
    givenSomeResourcesAndDisabledAnalyticsRuleForOneOfThem();
    afterRuleEnabling();
    makeSureResourceHasBeenAffected();
    makeSureEventTypeIsWathcedForResource();
}

TEST_F(RuleHolderTest, removeAllResourcesFromAnalyticsRule)
{
    givenAnalyticsRuleWithMultipleResources();
    afterRemovingAllResourcesFromRule();
    makeSureAllResourcesAreAffected();
    makeSureWatchedEventTypeListIsEmptyForAllResources();
}

TEST_F(RuleHolderTest, addResourceToAnalyticsRuleWithEmptyResourceList)
{
    givenAnalyticsRuleWithEmptyResourceList();
    afterAddingResourceToRuleResourceList();
    makeSureResourceHasBeenAffected();
    makeSureEventTypeIsWathcedForResource();
}

TEST_F(RuleHolderTest, createAnalyticsRuleFromNonAnalyticsRule)
{
    givenSomeResourcesAndNonAnalyticsRuleForOneOfThem();
    afterTurningToAnalyticsRule();
    makeSureResourceHasBeenAffected();
    makeSureEventTypeIsWathcedForResource();
}

TEST_F(RuleHolderTest, createNonAnalyticsRuleFromAnalyticsRule)
{
    givenSomeResourcesAndAnalyticsRuleForOneOfThem();
    afterTurningToNonAnalyticsRule();
    makeSureResourceHasBeenAffected();
    makeSureWatchedEventTypeListIsEmptyForResource();
}

} // namespace nx::vms::server::analytics
