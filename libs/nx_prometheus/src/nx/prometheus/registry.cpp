// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "registry.h"

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

struct Registry::Private
{
    explicit Private(details::Labels labels):
        constantLabels(std::move(labels))
    {
    }

    details::Registry registry;
    const details::Labels constantLabels;
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

std::string Registry::serialize() const
{
    return details::TextSerializer().Serialize(d->registry.Collect());
}

} // namespace nx::prometheus
