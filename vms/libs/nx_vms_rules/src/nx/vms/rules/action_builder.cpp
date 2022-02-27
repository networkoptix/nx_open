// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_builder.h"

#include <QtCore/QSet>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QVariant>

#include "action_field.h"

namespace nx::vms::rules {

using namespace std::chrono;

ActionBuilder::ActionBuilder(const QnUuid& id, const QString& actionType, const ActionConstructor& ctor):
    m_id(id),
    m_actionType(actionType),
    m_constructor(ctor)
{
    // TODO: #spanasenko Build m_targetFields list.
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ActionBuilder::onTimeout);
}

ActionBuilder::~ActionBuilder()
{
}

QnUuid ActionBuilder::id() const
{
    return m_id;
}

QString ActionBuilder::actionType() const
{
    return m_actionType;
}

void ActionBuilder::addField(const QString& name, std::unique_ptr<ActionField> field)
{
    // TODO: assert?
    m_fields[name] = std::move(field);
    updateState();
}

const QHash<QString, ActionField*> ActionBuilder::fields() const
{
    QHash<QString, ActionField*> result;
    for (const auto& [name, field]: m_fields)
    {
        result[name] = field.get();
    }
    return result;
}

QSet<QString> ActionBuilder::requiredEventFields() const
{
    QSet<QString> result;
    for (const auto& [name, field]: m_fields)
    {
        result += field->requiredEventFields();
    }
    return result;
}

QSet<QnUuid> ActionBuilder::affectedResources(const EventData& eventData) const
{
    QSet<QnUuid> result;
    for (auto field: m_targetFields)
    {
        auto value = field->build(eventData);
        result += value.toUuid(); //TODO: #spanasenko Check result.
    }
    return result;
}

void ActionBuilder::process(const EventData& eventData)
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

        for (const auto& [name, field]: m_fields)
        {
            const auto value = field->build(eventData);
            ptr->setProperty(name.toUtf8().data(), value);
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
    for (const auto& [name, field]: m_fields)
    {
        field->connectSignals();
        connect(field.get(), &Field::stateChanged, this, &ActionBuilder::updateState);
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
