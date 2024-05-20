// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/rules/running_event_watcher.h>

#include "test_event.h"

namespace nx::vms::rules::test {

TEST(RunningEventWatcherTest, eventWatcherProperlyHandlesDifferentEventsWithTheSameResourceKey)
{
    const auto startTime = std::chrono::microseconds{1};
    auto simpleEvent = TestEventProlongedPtr::create(startTime, State::started);
    auto testEvent = TestEventPtr::create(startTime, State::started);

    const auto cameraId = nx::Uuid::createUuid();
    simpleEvent->m_cameraId = cameraId;
    testEvent->m_cameraId = cameraId;

    RunningEventWatcher runningEventWatcher;
    EXPECT_FALSE(runningEventWatcher.isRunning(simpleEvent));
    EXPECT_EQ(runningEventWatcher.startTime(simpleEvent), std::chrono::microseconds::zero());
    EXPECT_FALSE(runningEventWatcher.isRunning(testEvent));
    EXPECT_EQ(runningEventWatcher.startTime(testEvent), std::chrono::microseconds::zero());

    runningEventWatcher.add(simpleEvent);
    EXPECT_TRUE(runningEventWatcher.isRunning(simpleEvent));
    EXPECT_EQ(runningEventWatcher.startTime(simpleEvent), startTime);
    EXPECT_FALSE(runningEventWatcher.isRunning(testEvent));
    EXPECT_EQ(runningEventWatcher.startTime(testEvent), std::chrono::microseconds::zero());

    runningEventWatcher.add(testEvent);
    EXPECT_TRUE(runningEventWatcher.isRunning(simpleEvent));
    EXPECT_EQ(runningEventWatcher.startTime(simpleEvent), startTime);
    EXPECT_TRUE(runningEventWatcher.isRunning(testEvent));
    EXPECT_EQ(runningEventWatcher.startTime(testEvent), startTime);

    const auto stopTime = std::chrono::microseconds{1000};;
    simpleEvent->setState(State::stopped);
    simpleEvent->setTimestamp(stopTime);
    testEvent->setState(State::stopped);
    testEvent->setTimestamp(stopTime);

    runningEventWatcher.erase(simpleEvent);
    EXPECT_FALSE(runningEventWatcher.isRunning(simpleEvent));
    EXPECT_EQ(runningEventWatcher.startTime(simpleEvent), std::chrono::microseconds::zero());
    EXPECT_TRUE(runningEventWatcher.isRunning(testEvent));
    EXPECT_EQ(runningEventWatcher.startTime(testEvent), startTime);

    runningEventWatcher.erase(testEvent);
    EXPECT_FALSE(runningEventWatcher.isRunning(simpleEvent));
    EXPECT_EQ(runningEventWatcher.startTime(simpleEvent), std::chrono::microseconds::zero());
    EXPECT_FALSE(runningEventWatcher.isRunning(testEvent));
    EXPECT_EQ(runningEventWatcher.startTime(testEvent), std::chrono::microseconds::zero());
}

} // namespace nx::vms::rules::test
