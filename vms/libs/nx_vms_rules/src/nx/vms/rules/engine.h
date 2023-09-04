// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QTimer>

#include <nx/utils/time/timer_event_handler.h>
#include <nx/utils/uuid.h>

#include "event_cache.h"
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
 * Works as factory for actual filters for the selected event type.
 * Works as factory for actual action builders for the selected action type.
 */
class NX_VMS_RULES_API Engine: public QObject
{
    Q_OBJECT

public:
    using RulePtr = std::shared_ptr<Rule>;
    using RuleSet = std::unordered_map<QnUuid, RulePtr>;
    using ConstRulePtr = std::shared_ptr<const Rule>;
    using ConstRuleSet = std::unordered_map<QnUuid, ConstRulePtr>;

    using EventConstructor = std::function<BasicEvent*()>;
    using EventFieldConstructor = std::function<EventFilterField*()>;

    using ActionConstructor = std::function<BasicAction*()>;
    using ActionFieldConstructor = std::function<ActionBuilderField*()>;

// Initialization and general info methods.
    Engine(std::unique_ptr<Router> router, QObject* parent = nullptr);
    ~Engine();

    void setId(QnUuid id);

    bool isEnabled() const;
    bool isOldEngineEnabled() const;

    bool addEventConnector(EventConnector* eventConnector);
    bool addActionExecutor(const QString& actionType, ActionExecutor* actionExecutor);

// Rule management methods.
    /** Returns all the rules engine contains. */
    ConstRuleSet rules() const;

    /** Returns rule with the given id or nullptr. */
    ConstRulePtr rule(const QnUuid& id) const;

    /** Returns a copy of the rule with the given id or nullptr. */
    RulePtr cloneRule(const QnUuid& id) const;

    /** Emits corresponding signal. */
    void updateRule(const api::Rule& ruleData);

    /** Resets all rules. Emits corresponding signal. */
    void resetRules(const std::vector<api::Rule>& rulesData);

    /** Removes existing rule from the engine, does nothing otherwise. Emits corresponding signal. */
    void removeRule(QnUuid ruleId);

// Event management methods.
    /**
     * Registers the event to the engine.
     * @return Whether the event was registered successfully.
     */
    bool registerEvent(const ItemDescriptor& descriptor, const EventConstructor& constructor);

    /** Map of all the registered events. */
    const QMap<QString, ItemDescriptor>& events() const;

    /**
     * @return Event description by event id if the event is registered, std::nullopt otherwise.
     */
    std::optional<ItemDescriptor> eventDescriptor(const QString& id) const;
    Group eventGroups() const;

    /**
    * Builds an event from the eventData and returns it.
    * If the eventData doesn't contains 'type' value or there is no constructor registered
    * for such type, nullptr is returned.
    */
    EventPtr buildEvent(const EventData& eventData) const;
    EventPtr cloneEvent(const EventPtr& event) const;

    bool isEventFieldRegistered(const QString& fieldId) const;
    bool registerEventField(const QString& type, const EventFieldConstructor& ctor);

    /**
    * Builds event filter based on the registered event descriptor. Filter fields are filled
    * with the default values. If the event descriptor or any of the fields is not registered, nullptr
    * is returned.
    */
    std::unique_ptr<EventFilter> buildEventFilter(const QString& eventType) const;

// Action managements methods.
    /**
     * Registers the action to the engine.
     * @return Whether the action is registered successfully.
     */
    bool registerAction(const ItemDescriptor& descriptor, const ActionConstructor& constructor);

    /** Map of all the registered actions. */
    const QMap<QString, ItemDescriptor>& actions() const;

    /**
     * @return Action description by event id if the event is registered, std::nullopt otherwise.
     */
    std::optional<ItemDescriptor> actionDescriptor(const QString& id) const;

    ActionPtr buildAction(const EventData& data) const;
    ActionPtr cloneAction(const ActionPtr& action) const;

    bool isActionFieldRegistered(const QString& fieldId) const;
    bool registerActionField(const QString& type, const ActionFieldConstructor& ctor);

    /**
     * Builds action builder based on the registered action descriptor. Builder fields are filled
     * with the default values. If the action descriptor or any of the fields not registered nullptr
     * is returned.
     */
    std::unique_ptr<ActionBuilder> buildActionBuilder(const QString& actionType) const;

// Runtime event processing methods.
    /** Processes incoming event and returns matched rule count. */
    size_t processEvent(const EventPtr& event);

    /** Processes incoming analytics events and returns matched rule count. */
    size_t processAnalyticsEvents(const std::vector<EventPtr>& events);

    EventCache* eventCache() const;

    void toggleTimer(nx::utils::TimerEventHandler* handler, bool on);

signals:
    void ruleAddedOrUpdated(QnUuid ruleId, bool added);
    void ruleRemoved(QnUuid ruleId);
    void rulesReset();

    void actionBuilt(
        const nx::vms::rules::AggregatedEventPtr& event,
        const nx::vms::rules::ActionPtr& action) const;

public: // Declare following methods public for testing purposes.
    /**
    * Builds default action field based on the registered action field constructor. Returns
    * nullptr if the event constructor is not registered.
    */
    std::unique_ptr<ActionBuilderField> buildActionField(const QString& fieldType) const;
    std::unique_ptr<ActionBuilder> buildActionBuilder(const api::ActionBuilder& serialized) const;

    /**
    * Builds default event field based on the registered event field constructor. Returns nullptr
    * if the event constructor is not registered.
    */
    std::unique_ptr<EventFilterField> buildEventField(const QString& fieldType) const;
    std::unique_ptr<EventFilter> buildEventFilter(const api::EventFilter& serialized) const;

private:
    // Assuming cache access and event processing are in the same thread.
    void checkOwnThread() const;

    std::unique_ptr<Rule> buildRule(const api::Rule& ruleData) const;
    std::unique_ptr<Rule> cloneRule(const Rule* rule) const;

    void processAcceptedEvent(const QnUuid& ruleId, const EventData& eventData);
    void processAction(const ActionPtr& action);

    std::unique_ptr<EventFilter> buildEventFilter(const ItemDescriptor& descriptor) const;
    std::unique_ptr<EventFilter> buildEventFilter(QnUuid id, const QString& type) const;
    std::unique_ptr<EventFilterField> buildEventField(const api::Field& serialized) const;

    std::unique_ptr<ActionBuilder> buildActionBuilder(const ItemDescriptor& descriptor) const;
    std::unique_ptr<ActionBuilder> buildActionBuilder(QnUuid id, const QString& type) const;
    std::unique_ptr<ActionBuilderField> buildActionField(const api::Field& serialized) const;

    /**
    * Registers the event constructor to the engine.
    * @return Whether the event constructor is registered successfully.
    */
    bool registerEventConstructor(const QString& id, const EventConstructor& constructor);
    /**
    * Registers the action constructor to the engine.
    * @return Whether the action constructor is registered successfully.
    */
    bool registerActionConstructor(const QString& id, const ActionConstructor& constructor);

private:
    bool m_enabled = false;
    bool m_oldEngineEnabled = true;

    QnUuid m_id;
    std::unique_ptr<Router> m_router;

    QList<QPointer<EventConnector>> m_connectors;
    QHash<QString, QPointer<ActionExecutor>> m_executors;

    QHash<QString, EventFieldConstructor> m_eventFields;
    QHash<QString, ActionFieldConstructor> m_actionFields;

    QHash<QString, EventConstructor> m_eventTypes;
    QHash<QString, ActionConstructor> m_actionTypes;

    QMap<QString, ItemDescriptor> m_eventDescriptors;
    QMap<QString, ItemDescriptor> m_actionDescriptors;

    // All the fields above are initialized on startup and remain unmodified at runtime.
    // Fields in the following section are guarded by mutex.
    mutable nx::Mutex m_ruleMutex;
    RuleSet m_rules;
    QSet<nx::utils::TimerEventHandler*> m_timerHandlers;

    // All the fields below should be used by Engine's thread only.
    std::unique_ptr<EventCache> m_eventCache;

    QTimer* m_aggregationTimer;
};

} // namespace nx::vms::rules
