// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <unordered_map>

#include <QList>
#include <QObject>
#include <QPointer>

#include <nx/utils/singleton.h>

#include "basic_event.h"
#include "basic_action.h"
#include "router.h"
#include "rule.h"

namespace nx::vms::api::rules {
    struct ActionBuilder;
    struct EventFilter;
    struct Field;
    struct Rule;
} // namespace nx::vms::api::rules

namespace nx::vms::rules {

class EventField;
class ActionField;

class EventConnector;
class ActionExecutor;

class BasicAction;

class ActionBuilder;
class EventFilter;

namespace api = ::nx::vms::api::rules;

/**
 * Central class in the VMS Rules hierarchy. Administers all the rules. Knowns all about event and
 * action types. Dispatches incoming events and triggers related actions by rules.
 * Stores all rules.
 * Works as factory for actual event and action fields. Their constructors are registered externally
 * and then used when we need to instantiate actual event filter or action builder.
 * Works as factory for actual filters for the selected event type. //< TODO: IMPL!
 * Works as factory for actual action builders for the selected action type. //< TODO: IMPL!
 */
class NX_VMS_RULES_API Engine:
    public QObject,
    public Singleton<Engine>
{
    Q_OBJECT

public:
    Engine(std::unique_ptr<Router> router, QObject* parent = 0);

    void init(const QnUuid& id, const std::vector<api::Rule>& rules);

    bool addEventConnector(EventConnector* eventConnector);
    bool addActionExecutor(const QString& actionType, ActionExecutor* actionExecutor);

    bool addRule(const api::Rule& serialized);

    //template<class T>
    //inline bool registerEventField()
    //{
    //    return registerEventField(T::manifest(), T::checker());
    //}

public:
    using EventFieldConstructor = std::function<EventField*()>;
    using ActionFieldConstructor = std::function<ActionField*()>;
    using ActionConstructor = std::function<BasicAction*()>;

    bool registerActionType(const QString& type, const ActionConstructor& ctor);

    bool registerEventField(const QString& type, const EventFieldConstructor& ctor);
    bool registerActionField(const QString& type, const ActionFieldConstructor& ctor);

private: //< ?
    bool registerEventField(
        const QJsonObject& manifest,
        const std::function<bool()>& checker);

    bool registerActionField(
        const QJsonObject& manifest,
        const std::function<QJsonValue()>& reducer);

    bool registerEventType(
        const QJsonObject& manifest,
        const std::function<EventPtr()>& constructor/*, QObject* source*/);

    bool registerActionType(
        const QJsonObject& manifest,
        const std::function<EventPtr()>& constructor,
        QObject* executor);

    std::unique_ptr<Rule> buildRule(const api::Rule& serialized) const;
    api::Rule serialize(const Rule* rule) const;

private:
    std::unique_ptr<EventFilter> buildEventFilter(const api::EventFilter& serialized) const;
    api::EventFilter serialize(const EventFilter* filter) const;

    std::unique_ptr<ActionBuilder> buildActionBuilder(const api::ActionBuilder& serialized) const;
    api::ActionBuilder serialize(const ActionBuilder *builder) const;

    std::unique_ptr<EventField> buildEventField(const api::Field& serialized) const;
    //api::Field serialize(const EventField* field) const;

    std::unique_ptr<ActionField> buildActionField(const api::Field& serialized) const;
    //api::Field serialize(const ActionField* field) const;

private:
    void processEvent(const EventPtr& event);
    void processAcceptedEvent(const QnUuid& ruleId, const EventData& eventData);
    void processAction(const ActionPtr& action);

private:
    QnUuid m_id;
    QList<QPointer<EventConnector>> m_connectors;
    QHash<QString, QPointer<ActionExecutor>> m_executors;

    QHash<QString, EventFieldConstructor> m_eventFields;
    QHash<QString, ActionFieldConstructor> m_actionFields;

    QHash<QString, ActionConstructor> m_actionTypes;

    std::unordered_map<QnUuid, std::unique_ptr<Rule>> m_rules;

    std::unique_ptr<Router> m_router;
};

} // namespace nx::vms::rules