// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <unordered_map>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::api::rules {
    struct ActionBuilder;
    struct EventFilter;
    struct Field;
    struct Rule;
} // namespace nx::vms::api::rules

namespace nx::vms::rules {

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
    using RuleSet = std::unordered_map<QnUuid, std::unique_ptr<Rule>>;

public:
    Engine(std::unique_ptr<Router> router, QObject* parent = nullptr);
    ~Engine();

    void init(const QnUuid& id, const std::vector<api::Rule>& rules);

    bool isEnabled() const;
    bool isOldEngineEnabled() const;
    bool hasRules() const;

    bool addEventConnector(EventConnector* eventConnector);
    bool addActionExecutor(const QString& actionType, ActionExecutor* actionExecutor);

    /** Returns all the rules engine contains. */
    const std::unordered_map<QnUuid, const Rule*> rules() const;

    /** Returns rule with the given id or nullptr. */
    const Rule* rule(const QnUuid& id) const;

    /**
     * Creates a copy of Rules stored inside the Engine.
     * This cloned state may be used to edit rules and to update Engine state later.
     */
    RuleSet cloneRules() const;

    void setRules(RuleSet&& rules);

    //template<class T>
    //inline bool registerEventField()
    //{
    //    return registerEventField(T::manifest(), T::checker());
    //}

public:
    using EventConstructor = std::function<BasicEvent*()>;
    using ActionConstructor = std::function<BasicAction*()>;

    /**
     * Registers the event to the engine.
     * @return Whether the event was registered successfully.
     */
    bool registerEvent(const ItemDescriptor& descriptor);

    /** Map of all the registered events. */
    const QMap<QString, ItemDescriptor>& events() const;

    /**
     * @return Event description by event id if the event is registered, std::nullopt otherwise.
     */
    std::optional<ItemDescriptor> eventDescriptor(const QString& id) const;

    /**
     * Registers the action to the engine.
     * @return Whether the action is registered successfully.
     */
    bool registerAction(const ItemDescriptor& descriptor, const ActionConstructor& constructor = {});

    /**
     * Registers the action constructor to the engine.
     * @return Whether the action constructor is registered successfully.
     */
    bool registerActionConstructor(const QString& id, const ActionConstructor& constructor);

    /** Map of all the registered actions. */
    const QMap<QString, ItemDescriptor>& actions() const;

    /**
     * @return Action description by event id if the event is registered, std::nullopt otherwise.
     */
    std::optional<ItemDescriptor> actionDescriptor(const QString& id) const;

public:
    using EventFieldConstructor = std::function<EventField*()>;
    using ActionFieldConstructor = std::function<ActionField*()>;

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

    bool addRule(const api::Rule& serialized);

public:
    // Declare following methods public for testing purposes.
    std::unique_ptr<Rule> buildRule(const api::Rule& serialized, bool embed) const;
    api::Rule serialize(const Rule* rule) const;

    std::unique_ptr<EventFilter> buildEventFilter(const api::EventFilter& serialized) const;
    api::EventFilter serialize(const EventFilter* filter) const;

    /**
     * Builds event filter based on the registered event descriptor. Filter fields are filled
     * with the default values. If the event descriptor or any of the fields not registered nullptr
     * is returned.
     */
    std::unique_ptr<EventFilter> buildEventFilter(const QString& eventType) const;

    std::unique_ptr<ActionBuilder> buildActionBuilder(const api::ActionBuilder& serialized) const;
    api::ActionBuilder serialize(const ActionBuilder *builder) const;

    /**
     * Builds action builder based on the registered action descriptor. Builder fields are filled
     * with the default values. If the action descriptor or any of the fields not registered nullptr
     * is returned.
     */
    std::unique_ptr<ActionBuilder> buildActionBuilder(const QString& actionType) const;

    std::unique_ptr<EventField> buildEventField(const api::Field& serialized) const;

    /**
     * Builds default event field based on the registered event field constructor. Returns nullptr
     * if the event constructor is not registered.
     */
    std::unique_ptr<EventField> buildEventField(const QString& fieldType) const;

    //api::Field serialize(const EventField* field) const;

    std::unique_ptr<ActionField> buildActionField(const api::Field& serialized) const;
    //api::Field serialize(const ActionField* field) const;

    /**
     * Builds default action field based on the registered action field constructor. Returns
     * nullptr if the event constructor is not registered.
     */
    std::unique_ptr<ActionField> buildActionField(const QString& fieldType) const;

private:
    void processEvent(const EventPtr& event);
    void processAcceptedEvent(const QnUuid& ruleId, const EventData& eventData);
    void processAction(const ActionPtr& action);

    std::unique_ptr<EventFilter> buildEventFilter(const ItemDescriptor& descriptor) const;
    std::unique_ptr<ActionBuilder> buildActionBuilder(const ItemDescriptor& descriptor) const;

    bool isEventFieldRegistered(const QString& fieldId) const;
    bool isActionFieldRegistered(const QString& fieldId) const;

private:
    bool m_enabled = false;
    bool m_oldEngineEnabled = true;

    QnUuid m_id;
    QList<QPointer<EventConnector>> m_connectors;
    QHash<QString, QPointer<ActionExecutor>> m_executors;

    QHash<QString, EventFieldConstructor> m_eventFields;
    QHash<QString, ActionFieldConstructor> m_actionFields;

    QHash<QString, ActionConstructor> m_actionTypes;

    RuleSet m_rules;

    std::unique_ptr<Router> m_router;

    QMap<QString, ItemDescriptor> m_eventDescriptors;
    QMap<QString, ItemDescriptor> m_actionDescriptors;
};

} // namespace nx::vms::rules
