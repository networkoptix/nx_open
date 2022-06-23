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
#include "basic_event.h"
#include "utils/field.h"

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

QnUuid ActionBuilder::ruleId() const
{
    return m_ruleId;
}

void ActionBuilder::setRuleId(const QnUuid& ruleId)
{
    m_ruleId = ruleId;
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
    if (!NX_ASSERT(field))
        return;

    if (name == utils::kIntervalFieldName)
    {
        auto optionalTimeField = dynamic_cast<OptionalTimeField*>(field.get());
        if (NX_ASSERT(optionalTimeField))
            setAggregationInterval(seconds(optionalTimeField->value()));
    }

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

QSet<QnUuid> ActionBuilder::affectedResources(const EventPtr& event) const
{
    NX_ASSERT(false, "Not implemented");
    return {};
}

void ActionBuilder::process(EventPtr event)
{
    if (!m_event)
        m_event = event;
    else
        m_event->aggregate(event);

    if (m_interval.count())
    {
        if (!m_timer.isActive())
        {
            m_timer.start();
            emit action(buildAction());
        }
    }
    else
    {
        emit action(buildAction());
    }
}

void ActionBuilder::setAggregationInterval(seconds interval)
{
    m_interval = interval;
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
    if (!m_event)
    {
        m_timer.start();
        emit action(buildAction());
    }
}

void ActionBuilder::updateState()
{
    //TODO: #spanasenko Update derived values (error messages, etc.)

    if (m_updateInProgress)
        return;

    QScopedValueRollback<bool> guard(m_updateInProgress, true);
    emit stateChanged();
}

ActionPtr ActionBuilder::buildAction()
{
    EventPtr event = std::move(m_event);

    if (!NX_ASSERT(m_constructor) || !NX_ASSERT(event))
        return {};

    ActionPtr action(m_constructor());
    if (!action)
        return {};

    action->setRuleId(m_ruleId);

    const auto propertyNames =
        nx::utils::propertyNames(action.get(), nx::utils::PropertyAccess::writable);
    for (const auto& propertyName: propertyNames)
    {
        const auto propertyNameUtf8 = propertyName.toUtf8();

        if (m_fields.contains(propertyName))
        {
            auto& field = m_fields.at(propertyName);
            const auto value = field->build(event);
            action->setProperty(propertyNameUtf8, value);
        }
        else
        {
            // Set property value only if it exists.
            if (action->property(propertyNameUtf8).isValid()
                && event->property(propertyNameUtf8).isValid())
            {
                action->setProperty(propertyNameUtf8, event->property(propertyNameUtf8));
            }
        }
    }

    return action;
}

} // namespace nx::vms::rules
