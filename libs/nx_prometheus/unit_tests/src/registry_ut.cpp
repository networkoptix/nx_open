// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <stdexcept>

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

} // namespace nx::prometheus::test
