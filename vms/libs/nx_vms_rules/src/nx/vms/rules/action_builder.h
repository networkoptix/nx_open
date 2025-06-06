// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariant>

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

    ActionBuilder(nx::Uuid id, const QString& actionType, const ActionConstructor& ctor);
    virtual ~ActionBuilder() override;

    nx::Uuid id() const;
    QString actionType() const;

    const Rule* rule() const;
    void setRule(const Rule* rule);

    // Takes ownership.
    void addField(const QString& name, std::unique_ptr<ActionBuilderField> field);

    QHash<QString, ActionBuilderField*> fields() const;

    /**
     * Process the event and emits action() signal whenever appropriate action is
     * built. Depends on the aggregation interval the event could be processed immediately
     * or aggregated and processed after the aggregation interval is triggered.
     */
    void process(const EventPtr& event);

    std::chrono::microseconds aggregationInterval() const;

    template<class T = Field>
    const T* fieldByName(const QString& name) const
    {
        return fieldByNameImpl<T>(name);
    }

    template<class T = Field>
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
    /** Emitted whenever any field property is changed. */
    void changed(const QString& fieldName);

    void action(const ActionPtr& action);

private slots:
    void onFieldChanged();

protected:
    void processEvent(const EventPtr& event);
    void buildAndEmitAction(const AggregatedEventPtr& aggregatedEvent);
    void handleAggregatedEvents();

private:
    virtual void onTimer(const nx::utils::TimerId& timerId) override;

    void setAggregationInterval(std::chrono::microseconds interval);
    void toggleAggregationTimer(bool on);
    Actions buildActionsForTargetUsers(const AggregatedEventPtr& aggregatedEvent) const;
    Actions buildActionsForAdditionalRecipients(const AggregatedEventPtr& aggregatedEvent) const;
    ActionPtr buildAction(const AggregatedEventPtr& event, const QVariantMap& override = {}) const;
    Engine* engine() const;
    bool isProlonged() const;

    nx::Uuid m_id;
    QString m_actionType;
    ActionConstructor m_constructor;
    const Rule* m_rule = {};
    std::map<QString, std::unique_ptr<ActionBuilderField>> m_fields;

    std::chrono::microseconds m_interval = std::chrono::microseconds::zero();
    QSharedPointer<Aggregator> m_aggregator;

    bool m_timerActive = false;

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
