#include <gtest/gtest.h>

#include <map>
#include <chrono>
#include <thread>

#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/core/resource/device_mock.h>

#include <nx/core/access/access_types.h>
#include <nx/vms/event/events/events.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::event;
using namespace std::chrono;
using namespace std::literals::chrono_literals;

using DeviceMock = nx::core::resource::DeviceMock;
using DeviceMockPtr = nx::core::resource::DeviceMockPtr;

static const QString kDefaultAnalyticsSdkEventTypeId = "test.default";
static const QString kCameraInputAnalyticsSdklEventTypeId = "test.input";

static const std::map<nx::vms::api::EventType, QString> kAnalyticsSdkEventTypeMapping = {
    {nx::vms::api::EventType::cameraInputEvent, kCameraInputAnalyticsSdklEventTypeId}
};

class EventRuleWatcherTest: public ::testing::Test
{

protected:
    virtual void SetUp()
    {
        m_commonModule = std::make_unique<QnCommonModule>(
            /*clientMode*/ false,
            nx::core::access::Mode::direct);

        m_eventRuleWatcher = std::make_unique<EventRuleWatcher>(
            m_commonModule->eventRuleManager());
    }

    void givenResource()
    {
        m_device = makeResource();
    }

    void givenSingleResourceRule(
        nx::vms::api::EventType eventType,
        const QString& analyticsSdkEventType = QString())
    {
        m_commonModule
            ->eventRuleManager()
            ->addOrUpdateRule(makeRule(m_device, eventType, analyticsSdkEventType));
    }

    void givenAnyResourceRule(
        nx::vms::api::EventType eventType,
        const QString& analyticsSdkEventType = QString())
    {
        m_commonModule
            ->eventRuleManager()
            ->addOrUpdateRule(makeRule(QnResourcePtr(), eventType, analyticsSdkEventType));
    }

    void afterRemovingRule()
    {
        m_commonModule->eventRuleManager()->removeRule(m_rule->id());
    }

    void afterEnablingRule()
    {
        m_rule->setDisabled(false);
        m_commonModule->eventRuleManager()->addOrUpdateRule(m_rule);
    }

    void afterDisablingRule()
    {
        m_rule->setDisabled(true);
        m_commonModule->eventRuleManager()->addOrUpdateRule(m_rule);
    }

    void makeSureEventTypeIsWatched(const QString& analyticsSdkEventType)
    {
        const std::set<QString> watchedEventTypes = m_eventRuleWatcher
            ->watchedEventsForDevice(m_device);

        ASSERT_TRUE(watchedEventTypes.find(analyticsSdkEventType) != watchedEventTypes.cend());
    }

    void makeSureEventTypeIsNotWatched(const QString& analyticsSdkEventType)
    {
        const std::set<QString> watchedEventTypes = m_eventRuleWatcher
            ->watchedEventsForDevice(m_device);

        ASSERT_TRUE(watchedEventTypes.find(analyticsSdkEventType) == watchedEventTypes.cend());
    }

    void makeSureWatchedEventListIsEmpty()
    {
        const std::set<QString> watchedEventTypes = m_eventRuleWatcher
            ->watchedEventsForDevice(m_device);

        ASSERT_TRUE(m_eventRuleWatcher->watchedEventsForDevice(m_device).empty());
    }

private:
    QnVirtualCameraResourcePtr makeResource()
    {
        DeviceMockPtr resource(new DeviceMock(m_commonModule.get()));
        resource->setAnalyticsSdkEventMapping(kAnalyticsSdkEventTypeMapping);

        return resource;
    }

    RulePtr makeRule(
        const QnResourcePtr& resource,
        nx::vms::api::EventType eventType,
        const QString& analyticsSdkEventTypeId)
    {
        RulePtr rule(new Rule());
        rule->setId(QnUuid::createUuid());
        rule->setEventType(eventType);

        if (resource)
            rule->setEventResources({resource->getId()});

        if (!analyticsSdkEventTypeId.isEmpty())
        {
            EventParameters eventParameters;
            eventParameters.setAnalyticsEventTypeId(analyticsSdkEventTypeId);
            rule->setEventParams(eventParameters);
        }

        m_rule = rule;

        return rule;
    }

private:
    RulePtr m_rule;
    QnVirtualCameraResourcePtr m_device;

    std::unique_ptr<QnCommonModule> m_commonModule;
    std::unique_ptr<EventRuleWatcher> m_eventRuleWatcher;
};

TEST_F(EventRuleWatcherTest, analyticsSdkEventIsWatchedAfterAddingRule)
{
    givenResource();
    givenSingleResourceRule(
        nx::vms::api::EventType::analyticsSdkEvent, kDefaultAnalyticsSdkEventTypeId);
    makeSureEventTypeIsWatched(kDefaultAnalyticsSdkEventTypeId);
}

TEST_F(EventRuleWatcherTest, analyticsSdkEventIsWatchedAfterAddingAnyResourceRule)
{
    givenResource();
    givenAnyResourceRule(
        nx::vms::api::EventType::analyticsSdkEvent, kDefaultAnalyticsSdkEventTypeId);
    makeSureEventTypeIsWatched(kDefaultAnalyticsSdkEventTypeId);
}

TEST_F(EventRuleWatcherTest, analyticsSdkEventIsNotWatchedAfterRemovingRule)
{
    givenResource();
    givenSingleResourceRule(
        nx::vms::api::EventType::analyticsSdkEvent, kDefaultAnalyticsSdkEventTypeId);
    afterRemovingRule();
    makeSureEventTypeIsNotWatched(kDefaultAnalyticsSdkEventTypeId);
}

TEST_F(EventRuleWatcherTest, analyticsSdkEventRuleDisableEnable)
{
    givenResource();
    givenSingleResourceRule(
        nx::vms::api::EventType::analyticsSdkEvent, kDefaultAnalyticsSdkEventTypeId);
    afterDisablingRule();
    makeSureEventTypeIsNotWatched(kDefaultAnalyticsSdkEventTypeId);
    afterEnablingRule();
    makeSureEventTypeIsWatched(kDefaultAnalyticsSdkEventTypeId);
}

TEST_F(EventRuleWatcherTest, analyticsSdkEventAnyResourceRuleDisableEnable)
{
    givenResource();
    givenAnyResourceRule(
        nx::vms::api::EventType::analyticsSdkEvent, kDefaultAnalyticsSdkEventTypeId);
    afterDisablingRule();
    makeSureEventTypeIsNotWatched(kDefaultAnalyticsSdkEventTypeId);
    afterEnablingRule();
    makeSureEventTypeIsWatched(kDefaultAnalyticsSdkEventTypeId);
}

TEST_F(EventRuleWatcherTest, analyticsSdkEventIsNotWatchedAfterRemovingAnyResourceRule)
{
    givenResource();
    givenAnyResourceRule(
        nx::vms::api::EventType::analyticsSdkEvent, kDefaultAnalyticsSdkEventTypeId);
    afterRemovingRule();
    makeSureEventTypeIsNotWatched(kDefaultAnalyticsSdkEventTypeId);
}

TEST_F(EventRuleWatcherTest, regularEventIsWatchedAfterAddingRule)
{
    givenResource();
    givenSingleResourceRule(nx::vms::api::EventType::cameraInputEvent);
    makeSureEventTypeIsWatched(kCameraInputAnalyticsSdklEventTypeId);
}

TEST_F(EventRuleWatcherTest, regularEventIsWatchedAfterAddingAnyResourceRule)
{
    givenResource();
    givenAnyResourceRule(nx::vms::api::EventType::cameraInputEvent);
    makeSureEventTypeIsWatched(kCameraInputAnalyticsSdklEventTypeId);
}

TEST_F(EventRuleWatcherTest, regularEventIsNotWatchedAfterRemovingRule)
{
    givenResource();
    givenSingleResourceRule(nx::vms::api::EventType::cameraInputEvent);
    afterRemovingRule();
    makeSureEventTypeIsNotWatched(kCameraInputAnalyticsSdklEventTypeId);
}

TEST_F(EventRuleWatcherTest, regularEventIsNotWatchedAfterRemovingAnyResourceRule)
{
    givenResource();
    givenAnyResourceRule(nx::vms::api::EventType::cameraInputEvent);
    afterRemovingRule();
    makeSureEventTypeIsNotWatched(kCameraInputAnalyticsSdklEventTypeId);
}

TEST_F(EventRuleWatcherTest, regularRuleDisableEnable)
{
    givenResource();
    givenSingleResourceRule(nx::vms::api::EventType::cameraInputEvent);
    afterDisablingRule();
    makeSureEventTypeIsNotWatched(kCameraInputAnalyticsSdklEventTypeId);
    afterEnablingRule();
    makeSureEventTypeIsWatched(kCameraInputAnalyticsSdklEventTypeId);
}

TEST_F(EventRuleWatcherTest, regularAnyResourceRuleDisableEnable)
{
    givenResource();
    givenSingleResourceRule(nx::vms::api::EventType::cameraInputEvent);
    afterDisablingRule();
    makeSureEventTypeIsNotWatched(kCameraInputAnalyticsSdklEventTypeId);
    afterEnablingRule();
    makeSureEventTypeIsWatched(kCameraInputAnalyticsSdklEventTypeId);
}

TEST_F(EventRuleWatcherTest, nonAnalyticsEventIsNotWatchedAfterAddingRule)
{
    givenResource();
    givenSingleResourceRule(nx::vms::api::EventType::cameraMotionEvent);
    makeSureWatchedEventListIsEmpty();
}

TEST_F(EventRuleWatcherTest, nonAnalyticsEventIsNotWatchedAfterAddingAnyResourceRule)
{
    givenResource();
    givenAnyResourceRule(nx::vms::api::EventType::cameraMotionEvent);
    makeSureWatchedEventListIsEmpty();
}

} // namespace nx::vms::server::analytics
