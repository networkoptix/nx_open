// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QObject>
#include <QTimer>

#include <nx/utils/uuid.h>

#include "basic_action.h"
#include "basic_event.h" // EventPtr

namespace nx::vms::rules {

class ActionField;
class BasicAction;

/**
 * Action builders are used to construct actual action parameters from action settings
 * contained in rules, and fields of events that triggered these rules.
 */
class NX_VMS_RULES_API /*FieldBased*/ActionBuilder: public QObject
{
    Q_OBJECT

public:
    using ActionConstructor = std::function<BasicAction*()>;

    ActionBuilder(const QnUuid& id, const QString& actionType, const ActionConstructor& ctor);
    virtual ~ActionBuilder();

    QnUuid id() const;
    QString actionType() const;

    // Takes ownership.
    void addField(const QString& name, std::unique_ptr<ActionField> field);

    const QHash<QString, ActionField*> fields() const;

    QSet<QString> requiredEventFields() const;
    QSet<QnUuid> affectedResources(const EventData& eventData) const;

    void process(const EventData& eventData);

    void setAggregationInterval(std::chrono::seconds interval);
    std::chrono::seconds aggregationInterval() const;

    void connectSignals();

signals:
    void action(const ActionPtr& action);
    void stateChanged();

private:
    void onTimeout();
    void updateState();

private:
    QnUuid m_id;
    QString m_actionType;
    ActionConstructor m_constructor;
    std::map<QString, std::unique_ptr<ActionField>> m_fields;
    QList<ActionField*> m_targetFields;
    std::chrono::seconds m_interval = std::chrono::seconds(0);
    QTimer m_timer;
    bool m_updateInProgress = false;
};

} // namespace nx::vms::rules