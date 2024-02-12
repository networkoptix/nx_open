// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

#include "field_types.h"
#include "rules_fwd.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

/**
 * Base class for storing data of output actions produced by action builders.
 * Derived classes should provide Q_PROPERTY's for all significant variables
 * (action fields) that are used to execute their type of action.
 */
class NX_VMS_RULES_API BasicAction: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type READ type CONSTANT FINAL)
    Q_PROPERTY(std::chrono::microseconds timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(nx::vms::api::rules::State state READ state WRITE setState)
    Q_PROPERTY(nx::Uuid ruleId READ ruleId WRITE setRuleId)

public:
    QString type() const;

    std::chrono::microseconds timestamp() const;
    void setTimestamp(std::chrono::microseconds timestamp);

    State state() const;
    void setState(State state);

    /** Returns a rule id the action belongs to. */
    nx::Uuid ruleId() const;
    void setRuleId(const nx::Uuid& ruleId);

    /**
     * Used to keep track of running prolonged actions.
     * Constructed from action type and some unique parameters.
     * Keep in sync with AbstractAction::getExternalUniqKey().
     * TODO: #amalov Carefully review and overload in descendant actions.
     */
    virtual QString uniqueKey() const;

    /** Returns formatted action details. */
    virtual QVariantMap details(common::SystemContext* context) const;

protected:
    BasicAction() = default;

private:
    std::chrono::microseconds m_timestamp = std::chrono::microseconds::zero();
    State m_state = State::instant;
    nx::Uuid m_ruleId;
};

} // namespace nx::vms::rules
