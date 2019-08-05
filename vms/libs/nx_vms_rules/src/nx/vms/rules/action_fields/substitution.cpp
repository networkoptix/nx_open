#include "substitution.h"

namespace nx::vms::rules {

Substitution::Substitution()
{
}

QVariant Substitution::build(const EventPtr& event) const
{
    const auto& value = event->property(m_eventFieldName.toUtf8().data());

    return value.canConvert(QVariant::String)
        ? value.toString()
        : QString("{%1}").arg(m_eventFieldName);
}

} // namespace nx::vms::rules
