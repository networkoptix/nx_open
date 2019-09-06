#include "action_builder.h"

#include <QScopedValueRollback>

#include "action_field.h"

namespace nx::vms::rules {

using namespace std::chrono;

ActionBuilder::ActionBuilder(const QnUuid& id, const QString& actionType, const ActionConstructor& ctor):
    m_id(id),
    m_actionType(actionType),
    m_constructor(ctor)
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

QString ActionBuilder::actionType() const
{
    return m_actionType;
}

void ActionBuilder::addField(const QString& name, ActionField* field)
{
    // TODO: assert?
    delete m_fields.value(name, nullptr);
    m_fields[name] = field;
    updateState();
}

const QHash<QString, ActionField*>& ActionBuilder::fields() const
{
   return m_fields;
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

void ActionBuilder::setAggregationInterval(seconds interval)
{
    m_interval = interval;

    m_timer.stop();
    m_timer.setInterval(m_interval);
    if (m_interval.count())
        m_timer.start();
    updateState();
}

seconds ActionBuilder::aggregationInterval() const
{
    return m_interval;
}

void ActionBuilder::connectSignals()
{
    for (auto& field: m_fields)
    {
        field->connectSignals();
        connect(field, &Field::stateChanged, this, &ActionBuilder::updateState);
    }
}

void ActionBuilder::onTimeout()
{
    // emit aggregated action
}

void ActionBuilder::updateState()
{
    //TODO: #spanasenko Update derived values (error messages, etc.)

    if (m_updateInProgress)
        return;

    QScopedValueRollback<bool> guard(m_updateInProgress, true);
    emit stateChanged();
}

} // namespace nx::vms::rules