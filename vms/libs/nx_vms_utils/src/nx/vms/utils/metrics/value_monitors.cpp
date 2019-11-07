#include "value_monitors.h"

namespace nx::vms::utils::metrics {

ValueMonitor::ValueMonitor(QString name, Scope scope):
    m_name(std::move(name)), m_scope(scope)
{}

QString ValueMonitor::name() const
{
    return m_name;
}

bool ValueMonitor::optional() const
{
    return m_optional;
}

void ValueMonitor::setOptional(bool isOptional)
{
    m_optional = isOptional;
}

Scope ValueMonitor::scope() const
{
    return m_scope;
}

void ValueMonitor::setScope(Scope scope)
{
    m_scope = scope;
}

api::metrics::Value ValueMonitor::value() const noexcept
{
    try
    {
        auto value = valueOrThrow();
        NX_ASSERT(!value.isNull() || m_optional, "The value %1 is unexpectedly null", this);
        return std::move(value);
    }
    catch (const MetricsError& e)
    {
        NX_ASSERT(false, "Got unexpected metric %1 error: %2", this, e.what());
    }
    catch (const std::exception& e)
    {
        NX_DEBUG(this, "Failed to get value: %1", e.what());
    }

    return {};
}

void ValueMonitor::setFormatter(ValueFormatter formatter)
{
    m_formatter = std::move(formatter);
}

api::metrics::Value ValueMonitor::formattedValue() const noexcept
{
    return m_formatter ? m_formatter(value()) : value();
}

QString ValueMonitor::toString() const
{
    return lm("%1(%2)").args(m_name, m_scope);
}

QString ValueMonitor::idForToStringFromPtr() const
{
    return name();
}

} // namespace nx::vms::utils::metrics
