// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <nx/prometheus/registry.h>

namespace nx::prometheus::test {

namespace {

constexpr char kServiceName[] = "testService";
constexpr char kEnvironment[] = "dev";

int countOccurrences(const std::string& text, const std::string& substring)
{
    int count = 0;
    for (size_t position = text.find(substring);
        position != std::string::npos;
        position = text.find(substring, position + substring.size()))
    {
        ++count;
    }
    return count;
}

} // namespace

class PrometheusRegistry: public ::testing::Test
{
protected:
    Registry registry{kServiceName, kEnvironment};
};

TEST_F(PrometheusRegistry, constant_labels_are_present_on_every_series)
{
    registry.counter("requests_total", "Served requests").increment();
    registry.gauge("queue_size", "Queued items", {{"queue", "main"}}).set(5);

    const std::string serialized = registry.serialize();
    const int sampleLineCount = 2;
    EXPECT_EQ(
        countOccurrences(serialized, Registry::kLabelServiceName + "=\"" + kServiceName + "\""),
        sampleLineCount);
    EXPECT_EQ(
        countOccurrences(serialized, Registry::kLabelEnvironment + "=\"" + kEnvironment + "\""),
        sampleLineCount);
    EXPECT_NE(serialized.find("queue=\"main\""), std::string::npos);
}

TEST_F(PrometheusRegistry, counter_accumulates)
{
    auto counter = registry.counter("events_total", "Processed events");
    counter.increment();
    counter.increment(/*value*/ 2.5);

    EXPECT_DOUBLE_EQ(counter.value(), 3.5);
    EXPECT_NE(registry.serialize().find("# TYPE events_total counter"), std::string::npos);
}

TEST_F(PrometheusRegistry, same_name_and_labels_return_the_same_series)
{
    auto first = registry.counter("events_total", "Processed events", {{"kind", "a"}});
    auto second = registry.counter("events_total", "Processed events", {{"kind", "a"}});
    first.increment();
    second.increment();

    EXPECT_DOUBLE_EQ(first.value(), 2.0);
    EXPECT_DOUBLE_EQ(second.value(), 2.0);
}

TEST_F(PrometheusRegistry, same_name_different_labels_form_one_family)
{
    registry.counter("events_total", "Processed events", {{"kind", "a"}}).increment();
    registry.counter("events_total", "Processed events", {{"kind", "b"}}).increment();

    const std::string serialized = registry.serialize();
    EXPECT_EQ(countOccurrences(serialized, "# TYPE events_total counter"), 1);
    EXPECT_NE(serialized.find("kind=\"a\""), std::string::npos);
    EXPECT_NE(serialized.find("kind=\"b\""), std::string::npos);
}

TEST_F(PrometheusRegistry, same_name_different_type_throws)
{
    registry.counter("conflicting_metric", "Help");
    EXPECT_THROW(registry.gauge("conflicting_metric", "Help"), std::invalid_argument);
}

TEST_F(PrometheusRegistry, gauge_round_trip)
{
    auto gauge = registry.gauge("temperature", "Current temperature");
    gauge.set(10);
    gauge.increment(/*value*/ 5);
    gauge.decrement(/*value*/ 3);

    EXPECT_DOUBLE_EQ(gauge.value(), 12.0);
}

TEST_F(PrometheusRegistry, histogram_buckets_and_chrono_observation)
{
    auto histogram = registry.histogram(
        "operation_duration_seconds", "Operation duration", /*buckets*/ {0.1, 1.0});
    histogram.observe(std::chrono::milliseconds(250));

    const std::string serialized = registry.serialize();
    EXPECT_NE(serialized.find("le=\"0.1\""), std::string::npos);
    EXPECT_NE(serialized.find("le=\"1\""), std::string::npos);
    EXPECT_NE(serialized.find("le=\"+Inf\""), std::string::npos);
    const std::string constantLabels = std::string("{") + Registry::kLabelEnvironment + "=\"" + kEnvironment
        + "\"," + Registry::kLabelServiceName + "=\"" + kServiceName + "\"}";
    EXPECT_NE(
        serialized.find("operation_duration_seconds_sum" + constantLabels + " 0.25"),
        std::string::npos);
    EXPECT_NE(
        serialized.find("operation_duration_seconds_count" + constantLabels + " 1"),
        std::string::npos);
}

TEST_F(PrometheusRegistry, histogram_family_resolved_once_reuses_series_per_label_set)
{
    auto family = registry.histogramFamily(
        "request_duration_seconds",
        "Request duration",
        /*buckets*/ {0.1, 1.0});

    // Repeated withLabels() with equal labels must resolve to the same underlying series.
    family.withLabels({{"route", "/ping"}}).observe(0.2);
    family.withLabels({{"route", "/ping"}}).observe(0.3);
    // A distinct label set forms a separate series within the same family.
    family.withLabels({{"route", "/status"}}).observe(0.5);

    const std::string serialized = registry.serialize();
    // One family (resolved once), and exactly two series - the two /ping calls reused a series
    // rather than minting a third.
    EXPECT_EQ(countOccurrences(serialized, "# TYPE request_duration_seconds histogram"), 1);
    EXPECT_EQ(countOccurrences(serialized, "request_duration_seconds_count{"), 2);

    // The two /ping observations accumulated into one series (count 2). Constant labels are
    // serialized first, then the per-series labels in insertion order.
    const std::string constantLabels = std::string(Registry::kLabelEnvironment) + "=\""
        + kEnvironment + "\"," + Registry::kLabelServiceName + "=\"" + kServiceName + "\"";
    EXPECT_NE(
        serialized.find(
            "request_duration_seconds_count{" + constantLabels + ",route=\"/ping\"} 2"),
        std::string::npos);
    EXPECT_NE(serialized.find("route=\"/status\""), std::string::npos);
}

TEST_F(PrometheusRegistry, registered_collector_runs_on_every_serialize)
{
    auto gauge = registry.gauge("pending_items", "Pending items");
    int callCount = 0;
    const auto handle = registry.registerCollector(
        [&callCount, &gauge]()
        {
            ++callCount;
            gauge.set(callCount);
        });

    EXPECT_NE(registry.serialize().find("pending_items{"), std::string::npos);
    EXPECT_EQ(callCount, 1);
    registry.serialize();
    EXPECT_EQ(callCount, 2);
}

TEST_F(PrometheusRegistry, collector_value_is_sampled_at_scrape_time)
{
    auto gauge = registry.gauge("queue_depth", "Queued items");
    int source = 7;
    const auto handle = registry.registerCollector([&gauge, &source]() { gauge.set(source); });

    // The value changes after registration, so a scrape reflecting it proves the callback is
    // read at serialize() rather than at registration.
    source = 42;
    const std::string constantLabels = std::string("{") + Registry::kLabelEnvironment + "=\""
        + kEnvironment + "\"," + Registry::kLabelServiceName + "=\"" + kServiceName + "\"}";
    EXPECT_NE(
        registry.serialize().find("queue_depth" + constantLabels + " 42"), std::string::npos);
}

TEST_F(PrometheusRegistry, destroyed_handle_stops_the_collector)
{
    int callCount = 0;
    {
        const auto handle = registry.registerCollector([&callCount]() { ++callCount; });
        registry.serialize();
        EXPECT_EQ(callCount, 1);
    }

    registry.serialize();
    EXPECT_EQ(callCount, 1);
}

TEST_F(PrometheusRegistry, reset_handle_stops_the_collector)
{
    int callCount = 0;
    auto handle = registry.registerCollector([&callCount]() { ++callCount; });
    registry.serialize();
    EXPECT_EQ(callCount, 1);

    handle.reset();
    registry.serialize();
    EXPECT_EQ(callCount, 1);
}

TEST_F(PrometheusRegistry, moved_handle_keeps_the_collector_registered)
{
    int callCount = 0;
    auto handle = registry.registerCollector([&callCount]() { ++callCount; });
    auto moved = std::move(handle);
    registry.serialize();
    EXPECT_EQ(callCount, 1);

    moved.reset();
    registry.serialize();
    EXPECT_EQ(callCount, 1);
}

TEST_F(PrometheusRegistry, throwing_collector_does_not_break_the_scrape)
{
    registry.counter("events_total", "Processed events").increment();
    const auto throwing =
        registry.registerCollector([]() { throw std::runtime_error("collector failure"); });
    int callCount = 0;
    const auto healthy = registry.registerCollector([&callCount]() { ++callCount; });

    std::string serialized;
    ASSERT_NO_THROW(serialized = registry.serialize());
    EXPECT_NE(serialized.find("# TYPE events_total counter"), std::string::npos);
    EXPECT_EQ(callCount, 1);
}

TEST_F(PrometheusRegistry, reset_waits_for_a_collector_running_on_another_thread)
{
    std::atomic<bool> collectorEntered{false};
    std::atomic<bool> collectorLeft{false};
    auto handle = registry.registerCollector(
        [&collectorEntered, &collectorLeft]()
        {
            collectorEntered = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            collectorLeft = true;
        });

    std::thread scrape([this]() { registry.serialize(); });
    while (!collectorEntered)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // reset() must not return while the collector is still reading the object it captured.
    handle.reset();
    EXPECT_TRUE(collectorLeft);

    scrape.join();
}

} // namespace nx::prometheus::test
