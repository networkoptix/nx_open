// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace prometheus {

class Counter;
class Gauge;
class Histogram;
template<typename T>
class Family;

} // namespace prometheus

namespace nx::prometheus {

namespace details { using namespace ::prometheus; }

using Labels = std::map<std::string, std::string>;

// Metric handles are cheap to copy and thread-safe, but must not outlive the Registry that
// created them.

class NX_PROMETHEUS_API Counter
{
public:
    void increment(double value = 1.0);
    double value() const;

private:
    friend class Registry;
    explicit Counter(details::Counter* counter);

    details::Counter* m_counter = nullptr;
};

class NX_PROMETHEUS_API Gauge
{
public:
    void set(double value);
    void increment(double value = 1.0);
    void decrement(double value = 1.0);
    double value() const;

private:
    friend class Registry;
    explicit Gauge(details::Gauge* gauge);

    details::Gauge* m_gauge = nullptr;
};

class NX_PROMETHEUS_API Histogram
{
public:
    void observe(double value);

    // Records the duration in seconds.
    template<typename Rep, typename Period>
    void observe(std::chrono::duration<Rep, Period> duration)
    {
        observe(std::chrono::duration<double>(duration).count());
    }

private:
    friend class Registry;
    friend class HistogramFamily;
    explicit Histogram(details::Histogram* histogram);

    details::Histogram* m_histogram = nullptr;
};

/**
 * Handle to a histogram family (a metric name with its bucket layout) whose registry-wide
 * lookup has already been done. withLabels() resolves just the per-label-set series, which is
 * far cheaper than re-registering the family on every observation - see Registry::histogram.
 */
class NX_PROMETHEUS_API HistogramFamily
{
public:
    // Returns the series for the given labels, creating it on first use. The family's buckets
    // apply; for an already existing series the buckets of the first call win.
    Histogram withLabels(const Labels& labels = {}) const;

private:
    friend class Registry;
    HistogramFamily(details::Family<details::Histogram>* family, std::vector<double> buckets);

    details::Family<details::Histogram>* m_family = nullptr;
    std::vector<double> m_buckets;
};

/**
 * Collects Prometheus metrics of one application and serializes them into the text exposition
 * format for a /metrics scrape handler. serviceName and environment are attached as constant
 * labels to every series, following the conventions of the Go nxtelemetry package.
 *
 * All methods are thread-safe (delegated to prometheus-cpp internal locking).
 */
class NX_PROMETHEUS_API Registry
{
public:
    // Resource label names, must match the Go side (libs/go/tools/utils/nxtelemetry/telemetry.go).
    inline static const std::string kLabelServiceName = "service_name";
    inline static const std::string kLabelEnvironment = "deployment_environment";

    Registry(std::string serviceName, std::string environment);
    ~Registry();

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    // Repeated calls with the same name and labels return a handle to the same series.
    // The same name with a different metric type throws std::invalid_argument.
    Counter counter(const std::string& name, const std::string& help, const Labels& labels = {});
    Gauge gauge(const std::string& name, const std::string& help, const Labels& labels = {});

    // For an already existing name + labels series the buckets of the first call win.
    // Each call re-resolves the family; for repeated observations prefer histogramFamily.
    Histogram histogram(
        const std::string& name,
        const std::string& help,
        std::vector<double> buckets,
        const Labels& labels = {});

    // Resolves a histogram family once so that per-observation cost is only the per-label-set
    // series lookup, not a registry-wide re-registration. Prefer this on hot paths that record
    // many observations under the same name with varying labels.
    HistogramFamily histogramFamily(
        const std::string& name,
        const std::string& help,
        std::vector<double> buckets);

    std::string serialize() const;

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::prometheus
