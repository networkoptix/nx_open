// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_builder.h"

#include <QtCore/QJsonValue>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QSet>
#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>
#include <nx/utils/qobject.h>

#include "action_field.h"
#include "basic_action.h"

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

std::map<QString, QVariant> ActionBuilder::flatData() const
{
    std::map<QString, QVariant> result;
    for (const auto& [id, field]: m_fields)
    {
        const auto& fieldProperties = field->serializedProperties();
        for (auto it = fieldProperties.begin(); it != fieldProperties.end(); ++it)
        {
            const QString path = id + "/" + it.key();
            result[path] = it.value();
        }
    }
    return result;
}

bool ActionBuilder::updateData(const QString& path, const QVariant& value)
{
    const auto ids = path.split('/');
    if (ids.size() != 2)
        return false;
    if (m_fields.find(ids[0]) == m_fields.end())
        return false;

    m_fields[ids[0]]->setProperty(ids[1].toLatin1().data(), value);
    return true;
}

bool ActionBuilder::updateFlatData(const std::map<QString, QVariant>& data)
{
    std::vector<std::tuple<QString, QString, QVariant>> parsed;

    /* Check data. */
    for (const auto& [id, value]: data)
    {
        const auto ids = id.split('/');
        if (ids.size() != 2)
            return false;
        if (m_fields.find(ids[0]) == m_fields.end())
         return false;
        parsed.push_back({ids[0], ids[1], value});
    }

    /* Update. */
    for (const auto& [field, prop, value]: parsed)
    {
        m_fields[field]->setProperty(prop.toLatin1().data(), value);
    }
    return true;
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

ActionPtr ActionBuilder::process(const EventData& eventData)
{
    if (m_interval.count())
    {
        if (!m_timer.isActive())
            m_timer.start();

        // TODO: #amalov Aggregate the event.
        return {};
    }
    else
    {
        if (!NX_ASSERT(m_constructor))
            return {};

        ActionPtr action(m_constructor());
        if (!action)
            return {};

        for (const auto& propertyName: utils::propertyNames(action.get()))
        {
            if (m_fields.contains(propertyName))
            {
                auto& field = m_fields.at(propertyName);
                const auto value = field->build(eventData);
                action->setProperty(propertyName.toUtf8().data(), value);
            }
            else
            {
                // Set property value only if it exists.
                if (action->property(propertyName.toUtf8().data()).isValid())
                    action->setProperty(propertyName.toUtf8().data(), eventData.value(propertyName));
            }
        }

        return action;
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
