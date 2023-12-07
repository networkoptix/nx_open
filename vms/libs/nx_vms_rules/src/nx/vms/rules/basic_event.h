// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/utils/uuid.h>

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

    Q_PROPERTY(QString type READ type CONSTANT FINAL)
    Q_PROPERTY(std::chrono::microseconds timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(nx::vms::api::rules::State state READ state WRITE setState)

public:
    explicit BasicEvent(
        std::chrono::microseconds timestamp,
        State state = State::instant);

    QString type() const;

    /**
     * Detailed type information, which can be used for extended event log filtration.
     * Empty by default. Actual mostly for the analytics events and actions where it
     * contains plugin-based event/action type.
     */
    virtual QString subtype() const;

    std::chrono::microseconds timestamp() const;
    void setTimestamp(const std::chrono::microseconds& timestamp);

    State state() const;
    void setState(State state);

    QString source(common::SystemContext* context) const;

    /**
     * Key for more detailed event aggregation. Used in SendMailAction.
     * Returns event type by default.
     * Keep in sync with EventParameters::getParamsHash().
     * Consider renaming to aggregationSubKey().
     */
    virtual QString uniqueName() const;

    /**
     * Returns string represent event uniqueness dependent on the resource produced the event.
     * Used for caching and matching prolonged events.
     * Keep in sync with RuleProcessor::getResourceKey().
     */
    virtual QString resourceKey() const = 0;

    /**
     * Key for basic event aggregation.
     * Returns resourceKey() by default.
     * Keep in sync with eventKey in RuleProcessor::processInstantAction().
     */
    virtual QString aggregationKey() const;

    /**
     * Used for caching and limiting repeat of instant events.
     * Returns empty string by default.
     * Keep in sync with uniqueKey in RuleProcessor::checkEventCondition().
     */
    virtual QString cacheKey() const;

    /** Returns the event details(such as caption, description, timestamp, source etc.). */
    virtual QVariantMap details(common::SystemContext* context) const;

protected:
    BasicEvent() = default;

    QString name(common::SystemContext* context) const;
    QString extendedCaption(common::SystemContext* context) const;

    /**
    * Returns event source to display in notification tile and tooltip.
    * Keep in sync with AbstractEvent.getResource().
    */
    QnUuid sourceId() const;

private:
    std::chrono::microseconds m_timestamp = std::chrono::microseconds::zero();
    State m_state = State::none;
};

} // namespace nx::vms::rules
