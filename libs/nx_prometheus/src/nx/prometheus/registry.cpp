// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "registry.h"

#include <exception>
#include <mutex>
#include <utility>

#include <nx/utils/log/log.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>

namespace nx::prometheus {

void Counter::increment(double value)
{
    m_counter->Increment(value);
}

double Counter::value() const
{
    return m_counter->Value();
}

Counter::Counter(details::Counter* counter):
    m_counter(counter)
{
}

void Gauge::set(double value)
{
    m_gauge->Set(value);
}

void Gauge::increment(double value)
{
    m_gauge->Increment(value);
}

void Gauge::decrement(double value)
{
    m_gauge->Decrement(value);
}

double Gauge::value() const
{
    return m_gauge->Value();
}

Gauge::Gauge(details::Gauge* gauge):
    m_gauge(gauge)
{
}

void Histogram::observe(double value)
{
    m_histogram->Observe(value);
}

Histogram::Histogram(details::Histogram* histogram):
    m_histogram(histogram)
{
}

Histogram HistogramFamily::withLabels(const Labels& labels) const
{
    return Histogram(&m_family->Add(labels, m_buckets));
}

HistogramFamily::HistogramFamily(
    details::Family<details::Histogram>* family,
    std::vector<double> buckets):
    m_family(family),
    m_buckets(std::move(buckets))
{
}

CollectorHandle::CollectorHandle(Registry* registry, std::uint64_t id):
    m_registry(registry),
    m_id(id)
{
}

CollectorHandle::CollectorHandle(CollectorHandle&& other) noexcept:
    m_registry(std::exchange(other.m_registry, nullptr)),
    m_id(std::exchange(other.m_id, 0))
{
}

CollectorHandle& CollectorHandle::operator=(CollectorHandle&& other) noexcept
{
    if (this != &other)
    {
        reset();
        m_registry = std::exchange(other.m_registry, nullptr);
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

CollectorHandle::~CollectorHandle()
{
    reset();
}

void CollectorHandle::reset()
{
    if (m_registry)
        m_registry->deregisterCollector(m_id);
    m_registry = nullptr;
    m_id = 0;
}

struct Registry::Private
{
    explicit Private(details::Labels labels):
        constantLabels(std::move(labels))
    {
    }

    details::Registry registry;
    const details::Labels constantLabels;

    std::mutex collectorMutex;
    std::map<std::uint64_t, std::function<void()>> collectors;
    std::uint64_t nextCollectorId = 1;
};

Registry::Registry(std::string serviceName, std::string environment):
    d(std::make_unique<Private>(details::Labels{
        {kLabelServiceName, std::move(serviceName)},
        {kLabelEnvironment, std::move(environment)}}))
{
}

Registry::~Registry() = default;

Counter Registry::counter(const std::string& name, const std::string& help, const Labels& labels)
{
    auto& family = details::BuildCounter()
                       .Name(name)
                       .Help(help)
                       .Labels(d->constantLabels)
                       .Register(d->registry);
    return Counter(&family.Add(labels));
}

Gauge Registry::gauge(const std::string& name, const std::string& help, const Labels& labels)
{
    auto& family = details::BuildGauge()
                       .Name(name)
                       .Help(help)
                       .Labels(d->constantLabels)
                       .Register(d->registry);
    return Gauge(&family.Add(labels));
}

Histogram Registry::histogram(
    const std::string& name,
    const std::string& help,
    std::vector<double> buckets,
    const Labels& labels)
{
    return histogramFamily(name, help, std::move(buckets)).withLabels(labels);
}

HistogramFamily Registry::histogramFamily(
    const std::string& name,
    const std::string& help,
    std::vector<double> buckets)
{
    auto& family = details::BuildHistogram()
                       .Name(name)
                       .Help(help)
                       .Labels(d->constantLabels)
                       .Register(d->registry);
    return HistogramFamily(&family, std::move(buckets));
}

CollectorHandle Registry::registerCollector(std::function<void()> collector)
{
    std::lock_guard lock(d->collectorMutex);
    const auto id = d->nextCollectorId++;
    d->collectors.emplace(id, std::move(collector));
    return CollectorHandle(this, id);
}

void Registry::deregisterCollector(std::uint64_t id)
{
    std::lock_guard lock(d->collectorMutex);
    d->collectors.erase(id);
}

std::string Registry::serialize() const
{
    // Invoked under the lock so that deregisterCollector() blocks until an in-flight scrape has
    // finished: a handle destroyed on another thread must not let its owner die mid-callback.
    std::lock_guard lock(d->collectorMutex);
    for (const auto& entry: d->collectors)
    {
        try
        {
            entry.second();
        }
        catch (const std::exception& e)
        {
            NX_WARNING(this, "Metrics collector failed: %1", e.what());
        }
        catch (...)
        {
            NX_WARNING(this, "Metrics collector failed with an unknown exception");
        }
    }

    return details::TextSerializer().Serialize(d->registry.Collect());
}

} // namespace nx::prometheus
