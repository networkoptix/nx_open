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
#include <nx/vms/api/rules/rules_fwd.h>
#include <nx/vms/common/system_context_aware.h>

#include "event_cache.h"
#include "field_validator.h"
#include "rules_fwd.h"
#include "running_event_watcher.h"

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
class NX_VMS_RULES_API Engine:
    public QObject,
    public common::SystemContextAware
{
    Q_OBJECT

public:
    using RuleSet = std::unordered_map<nx::Uuid, RulePtr>;
    using ConstRuleSet = std::unordered_map<nx::Uuid, ConstRulePtr>;

    using EventConstructor = std::function<BasicEvent*()>;
    using EventFieldConstructor =
        std::function<EventFilterField*(const FieldDescriptor* descriptor)>;

    using ActionConstructor = std::function<BasicAction*()>;
    struct ActionFieldRecord
    {
        using Constructor = std::function<ActionBuilderField*(const FieldDescriptor* descriptor)>;
        Constructor constructor;
        QSet<QString> encryptedFields;
    };

// Initialization and general info methods.
    explicit Engine(
        common::SystemContext* systemContext,
        std::unique_ptr<Router> router,
        QObject* parent = nullptr);
    ~Engine();

    void setId(nx::Uuid id);
    Router* router() const;

    bool isEnabled() const;
    bool isOldEngineEnabled() const;

    bool addEventConnector(EventConnector* eventConnector);
    bool addActionExecutor(const QString& actionType, ActionExecutor* actionExecutor);

// Rule management methods.
    /** Returns all the rules engine contains. */
    ConstRuleSet rules() const;

    /** Returns rule with the given id or nullptr. */
    ConstRulePtr rule(nx::Uuid id) const;

    /** Returns a copy of the rule with the given id or nullptr. */
    RulePtr cloneRule(nx::Uuid id) const;

    /** Emits corresponding signal. */
    void updateRule(const api::Rule& ruleData);

    /** Resets all rules. Emits corresponding signal. */
    void resetRules(const std::vector<api::Rule>& rulesData);

    /** Builds Rule from API Rule. */
    std::unique_ptr<Rule> buildRule(const api::Rule& ruleData) const;

    /** Removes existing rule from the engine, does nothing otherwise. Emits corresponding signal. */
    void removeRule(nx::Uuid ruleId);

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
    bool registerActionField(const QString& type, const ActionFieldRecord& record);

    /** To be used with fixTransactionInputFromApi / amendOutputDataIfNeeded function pair. */
    const QSet<QString>& encryptedActionBuilderProperties(const QString& type) const;

    /**
     * Builds action builder based on the registered action descriptor. Builder fields are filled
     * with the default values. If the action descriptor or any of the fields not registered nullptr
     * is returned.
     */
    std::unique_ptr<ActionBuilder> buildActionBuilder(const QString& actionType) const;

// Field validator registration and access methods.

    bool registerValidator(const QString& fieldType, FieldValidator* validator);
    FieldValidator* fieldValidator(const QString& fieldType) const;

// Runtime event processing methods.
    /** Processes incoming event and returns matched rule count. */
    size_t processEvent(const EventPtr& event);

    /** Processes incoming analytics events and returns matched rule count. */
    size_t processAnalyticsEvents(const std::vector<EventPtr>& events);

    const EventCache& eventCache() const;
    const RunningEventWatcher& runningEventWatcher() const;

    void toggleTimer(nx::utils::TimerEventHandler* handler, bool on);

signals:
    void ruleAddedOrUpdated(nx::Uuid ruleId, bool added);
    void ruleRemoved(nx::Uuid ruleId);
    void rulesReset();

    void actionBuilt(
        const nx::vms::rules::AggregatedEventPtr& event,
        const nx::vms::rules::ActionPtr& action) const;

public: // Declare following methods public for testing purposes.
    /**
    * Builds default action field based on the registered action field constructor. Returns
    * nullptr if the event constructor is not registered.
    */
    std::unique_ptr<ActionBuilderField> buildActionField(const FieldDescriptor* descriptor) const;
    std::unique_ptr<ActionBuilder> buildActionBuilder(const api::ActionBuilder& serialized) const;

    /**
    * Builds default event field based on the registered event field constructor. Returns nullptr
    * if the event constructor is not registered.
    */
    std::unique_ptr<EventFilterField> buildEventField(const FieldDescriptor* descriptor) const;
    std::unique_ptr<EventFilter> buildEventFilter(const api::EventFilter& serialized) const;

private:
    // Assuming cache access and event processing are in the same thread.
    void checkOwnThread() const;

    // Check whether the given event should be processed. The method is checking if the event is
    // not cached, and if the event is prolonged, it checks that it has a valid state.
    bool checkEvent(const EventPtr& event) const;

    std::unique_ptr<Rule> cloneRule(const Rule* rule) const;

    void onEventReceved(const EventPtr& event, const std::vector<ConstRulePtr>& triggeredRules);
    void processAcceptedEvent(const EventPtr& event, const ConstRulePtr& rule);

    void processAction(const ActionPtr& action) const;
    void processAcceptedAction(const ActionPtr& action);

    std::unique_ptr<EventFilter> buildEventFilter(const ItemDescriptor& descriptor) const;
    std::unique_ptr<EventFilter> buildEventFilter(nx::Uuid id, const QString& type) const;
    std::unique_ptr<EventFilterField> buildEventField(
        const api::Field& serialized, const FieldDescriptor* descriptor) const;

    std::unique_ptr<ActionBuilder> buildActionBuilder(const ItemDescriptor& descriptor) const;
    std::unique_ptr<ActionBuilder> buildActionBuilder(nx::Uuid id, const QString& type) const;
    std::unique_ptr<ActionBuilderField> buildActionField(
        const api::Field& serialized, const FieldDescriptor* descriptor) const;

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

    void stopRunningActions(nx::Uuid ruleId);
    void stopRunningAction(const ActionPtr& action);

private:
    bool m_enabled = false;
    bool m_oldEngineEnabled = true;

    nx::Uuid m_id;
    std::unique_ptr<Router> m_router;

    QList<QPointer<EventConnector>> m_connectors;
    QHash<QString, QPointer<ActionExecutor>> m_executors;

    QHash<QString, EventFieldConstructor> m_eventFields;
    QHash<QString, ActionFieldRecord> m_actionFields;

    QHash<QString, FieldValidator*> m_fieldValidators;

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
    EventCache m_eventCache;
    RunningEventWatcher m_runningEventWatcher;

    // Running actions by their resources hash, grouped by rule Id.
    QHash<nx::Uuid, QHash<size_t, ActionPtr>> m_runningActions;

    QTimer* m_aggregationTimer;
};

} // namespace nx::vms::rules
