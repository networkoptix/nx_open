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
     * uniqueness is determinded by the event name.
     */
    virtual QString uniqueName() const;

    /**
     * Aggregates the event. If the event is unique, then uniqueEventCount increases,
     * totalEventCount otherwise.
     */
    virtual void aggregate(const EventPtr& event);

    /** Returns amount of the unique aggregated events. Uniqueness is determined by the uniqueName(). */
    size_t uniqueEventCount() const;

    /** Returns total amount of the aggregated events. */
    size_t totalEventCount() const;

    /** Returns the event string formatted details(such as caption, description, timestamp etc.). */
    virtual QMap<QString, QString> details(common::SystemContext* context) const;

protected:
    BasicEvent() = default;

    template<class... Strings>
    QString makeName(const Strings&... strings) const
    {
        static constexpr auto kEventNameSeparator = '_';
        return QStringList{strings...}.join(kEventNameSeparator);
    }

private:
    QString m_type;
    std::chrono::microseconds m_timestamp;
    State m_state = State::none;

    /** Holds events occurencies count. */
    std::unordered_map</*unique name*/ QString, /*count*/ size_t> m_eventsHash;

    QString name() const;
    QString aggregatedTimestamp() const;
};

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(nx::vms::rules::EventPtr)
