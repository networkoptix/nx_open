#include "action_builder.h"

#include "action_field.h"

namespace nx::vms::rules {

using namespace std::chrono;

ActionBuilder::ActionBuilder(const QnUuid& id, const ActionConstructor& ctor):
    m_id(id),
    m_constructor(ctor)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ActionBuilder::onTimeout);
}

ActionBuilder::~ActionBuilder()
{
    qDeleteAll(m_fields);
}

 bool ActionBuilder::addField(const QString& name, ActionField* field)
 {
    if (m_fields.contains(name))
        return false;

    m_fields[name] = field;
    return true;
 }

void ActionBuilder::process(const EventPtr& event)
{
    if (m_interval.count())
    {
        if (!m_timer.isActive())
            m_timer.start();

        // Aggregate the event.
    }
    else
    {
        ActionPtr ptr(m_constructor());
        if (!ptr)
            return;

        for (auto it = m_fields.begin(); it != m_fields.end(); ++it)
        {
            const auto& fieldName = it.key();
            const auto field = it.value();

            const auto value = field->build(event);
            ptr->setProperty(fieldName.toUtf8().data(), value);
        }

        emit action(ptr);
    }
}

QnUuid ActionBuilder::id() const
{
    return m_id;
}

void ActionBuilder::setAggregationInterval(seconds interval)
{
    m_interval = interval;

    m_timer.stop();
    m_timer.setInterval(m_interval);
    if (m_interval.count())
        m_timer.start();
}

seconds ActionBuilder::aggregationInterval() const
{
    return m_interval;
}

void ActionBuilder::onTimeout()
{
    // emit aggregated action
}

} // namespace nx::vms::rules