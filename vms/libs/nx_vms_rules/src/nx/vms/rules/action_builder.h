// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <nx/utils/time/timer_event_handler.h>
#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

class ActionBuilderField;
class Aggregator;

/**
 * Action builders are used to construct actual action parameters from action settings
 * contained in rules, and fields of events that triggered these rules.
 */
class NX_VMS_RULES_API ActionBuilder: public QObject, public nx::utils::TimerEventHandler
{
    Q_OBJECT

public:
    using ActionConstructor = std::function<BasicAction*()>;
    using Actions = std::vector<ActionPtr>;

    ActionBuilder(const QnUuid& id, const QString& actionType, const ActionConstructor& ctor);
    virtual ~ActionBuilder();

    QnUuid id() const;
    QString actionType() const;

    const Rule* rule() const;
    void setRule(const Rule* rule);

    /**
     * Get all configurable Builder properties in a form of flattened dictionary,
     * where each key has name in a format "field_name/field_property_name".
     */
    std::map<QString, QVariant> flatData() const;

    /**
     * Update single Builder property.
     * @path Path of the property, should has "field_name/field_property_name" format.
     * @value Value that should be assigned to the given property.
     * @return True on success, false if such field does not exist.
     */
    bool updateData(const QString& path, const QVariant& value);

    /**
     * Update several Builder properties at once.
     * Does nothing if any of the keys is invalid (e.g. such field does not exist).
     * @data Dictionary of values.
     * @return True on success, false otherwise.
     */
    bool updateFlatData(const std::map<QString, QVariant>& data);

    // Takes ownership.
    void addField(const QString& name, std::unique_ptr<ActionBuilderField> field);

    const QHash<QString, ActionBuilderField*> fields() const;

    QSet<QString> requiredEventFields() const;
    QSet<QnUuid> affectedResources(const EventPtr& event) const;

    /**
     * Process the event and emits action() signal whenever appropriate action is
     * built. Depends on the aggregation interval the event could be processed immediately
     * or aggregated and processed after the aggregation interval is triggered.
     */
    void process(const EventPtr& event);

    std::chrono::microseconds aggregationInterval() const;

    /** Action require separate start and end events.*/
    bool isProlonged() const;

    void connectSignals();

    template<class T>
    const T* fieldByName(const QString& name) const
    {
        return fieldByNameImpl<T>(name);
    }

    template<class T>
    T* fieldByName(const QString& name)
    {
        return fieldByNameImpl<T>(name);
    }

    template<class T>
    const T* fieldByType() const
    {
        return fieldByTypeImpl<T>();
    }

    template<class T>
    T* fieldByType()
    {
        return fieldByTypeImpl<T>();
    }

signals:
    void stateChanged();

    /** Emitted whenever any field property is changed. */
    void changed();

    void action(const ActionPtr& action);

protected:
    void processEvent(const EventPtr& event);
    void buildAndEmitAction(const AggregatedEventPtr& aggregatedEvent);
    void handleAggregatedEvents();

private:
    virtual void onTimer(const nx::utils::TimerId& timerId) override;

    void updateState();
    void setAggregationInterval(std::chrono::microseconds interval);
    void toggleAggregationTimer(bool on);
    Actions buildActionsForTargetUsers(const AggregatedEventPtr& aggregatedEvent);
    ActionPtr buildAction(const AggregatedEventPtr& aggregatedEvent);
    Engine* engine() const;


    QnUuid m_id;
    QString m_actionType;
    ActionConstructor m_constructor;
    const Rule* m_rule = {};
    std::map<QString, std::unique_ptr<ActionBuilderField>> m_fields;

    std::chrono::microseconds m_interval = std::chrono::microseconds::zero();
    QSharedPointer<Aggregator> m_aggregator;

    // Running flag for prolonged actions.
    bool m_isActionRunning = false;
    bool m_timerActive = false;
    bool m_updateInProgress = false;

    template<class T>
    T* fieldByNameImpl(const QString& name) const
    {
        const auto it = m_fields.find(name);
        return (it == m_fields.end()) ? nullptr : dynamic_cast<T*>(it->second.get());
    }

    /** Returns first field with the given type T. */
    template<class T>
    T* fieldByTypeImpl() const
    {
        for (const auto& [_, field]: m_fields)
        {
            if (auto targetField = dynamic_cast<T*>(field.get()))
                return targetField;
        }

        return {};
    }
};

} // namespace nx::vms::rules
