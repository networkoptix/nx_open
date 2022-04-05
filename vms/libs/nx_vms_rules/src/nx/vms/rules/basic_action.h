// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <nx/vms/api/rules/event_info.h>

#include "field_types.h"
#include "manifest.h"

namespace nx::vms::rules {

enum class ActionType
{
    instant = 0,
    actAtStart = 1 << 0,
    actAtEnd = 1 << 1,
    prolonged = actAtStart | actAtEnd
};

/**
 * Base class for storing data of output actions produced by action builders.
 * Derived classes should provide Q_PROPERTY's for all significant variables
 * (action fields) that are used to execute their type of action.
 */
class NX_VMS_RULES_API BasicAction: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::chrono::microseconds timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(QString eventType READ eventType WRITE setEventType)

public:
    QString type() const;

    std::chrono::microseconds timestamp() const;
    void setTimestamp(std::chrono::microseconds timestamp);
    QString eventType() const;
    void setEventType(const QString& eventType);

    /** Returns a rule id the action belongs to. */
    QnUuid ruleId() const;
    void setRuleId(const QnUuid& ruleId);

protected:
    BasicAction() = default;

private:
    std::chrono::microseconds m_timestamp;
    QString m_eventType;
    QnUuid m_ruleId;
};

} // namespace nx::vms::rules
