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
    return handleValueErrors([this](){ return this->valueOrThrow(); });
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

api::metrics::Value ValueMonitor::handleValueErrors(
    std::function<api::metrics::Value()> calculateValue) const
{
    try
    {
        auto value = calculateValue();
        NX_ASSERT(!value.isNull() || m_optional, "The value %1 is unexpectedly null", this);
        return std::move(value);
    }
    catch (const ExpectedError& e)
    {
        NX_DEBUG(this, "Got error: %1", nx::utils::unwrapNestedErrors(e));
    }
    catch (const MetricsError& e)
    {
        NX_ASSERT(false, "Got unexpected metric %1 error: %2", this, e.what());
    }
    catch (const std::exception& e)
    {
        NX_ASSERT(false, "Unexpected general error when calculating metric %1: %2", this, e.what());
    }
    // TODO: Should we return error to the user if it occures?
    return {};
}

} // namespace nx::vms::utils::metrics
