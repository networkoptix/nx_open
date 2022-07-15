// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <unordered_map>

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

#include <nx/vms/api/rules/event_info.h>

#include "manifest.h"
#include "rules_fwd.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules {

/**
 * Base class for storing data of input events produced by event connectors.
 * Derived classes should provide Q_PROPERTY's for all significant variables
 * (event fields) of their event type.
 */
class NX_VMS_RULES_API BasicEvent: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(std::chrono::microseconds timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(nx::vms::api::rules::State state READ state WRITE setState)

public:
    explicit BasicEvent(const nx::vms::api::rules::EventInfo& info);
    explicit BasicEvent(
        std::chrono::microseconds timestamp,
        State state = State::instant);

    QString type() const;

    std::chrono::microseconds timestamp() const;
    void setTimestamp(const std::chrono::microseconds& timestamp);

    State state() const;
    void setState(State state);

    /**
     * Returns the event unique name. Used for the event aggregation. At the basic level event
     * uniqueness is determined by the event name.
     * Keep in sync with EventParameters::getParamsHash().
     */
    virtual QString uniqueName() const;

    /**
     * Returns string represent event uniqueness dependent on the resource produced the event.
     * Used for caching and matching prolonged events.
     * Keep in sync with RuleProcessor::getResourceKey().
     */
    virtual QString resourceKey() const = 0;

    /**
     * Used for caching and limiting repeat of instant events.
     * Keep in sync with uniqueKey in RuleProcessor::checkEventCondition().
     */
    virtual QString cacheKey() const;

    /** Returns the event details(such as caption, description, timestamp, source etc.). */
    virtual QVariantMap details(common::SystemContext* context) const;

    QString name() const;

protected:
    BasicEvent() = default;

    template<class... Strings>
    QString makeName(const Strings&... strings) const
    {
        static constexpr auto kEventNameSeparator = '_';
        return QStringList{strings...}.join(kEventNameSeparator);
    }

    QString extendedCaption() const;

private:
    QString m_type;
    std::chrono::microseconds m_timestamp;
    State m_state = State::none;
};

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(nx::vms::rules::EventPtr)
