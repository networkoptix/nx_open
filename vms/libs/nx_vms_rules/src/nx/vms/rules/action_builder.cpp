#include "action_builder.h"

#include "action_field.h"

namespace nx::vms::rules {

using namespace std::chrono;

ActionBuilder::ActionBuilder(const QnUuid& id): m_id(id)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ActionBuilder::onTimeout);
}

ActionBuilder::~ActionBuilder()
{
    qDeleteAll(m_fields);
}

QnUuid ActionBuilder::id() const
{
    return m_id;
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
        emit action(ActionPtr(new BasicAction()));
    }
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