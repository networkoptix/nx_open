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
 * Base class for storing data of input events produced by event connectors. Derived classes must
 * provide Q_PROPERTY's for all significant variables (event fields) of their event type because
 * the event instance should be able to be rebuilt from the properties set.
 */
class NX_VMS_RULES_API BasicEvent: public QObject
{
    // Do not include FIELD macro header to avoid global macro propagation.
    Q_OBJECT
    Q_PROPERTY(QString type READ type CONSTANT FINAL)
    Q_PROPERTY(std::chrono::microseconds timestamp READ timestamp WRITE setTimestamp)
    Q_PROPERTY(nx::vms::api::rules::State state READ state WRITE setState)

public:
    explicit BasicEvent(std::chrono::microseconds timestamp, State state = State::instant);

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

    /**
     * Key for the event aggregation. Required to unite several sequentially incoming events into a
     * single AggregatedEvent instance. Usually it is either device or server id.
     */
    virtual QString aggregationKey() const = 0;

    /**
     * Returns string representing event uniqueness. It is needed to distinguish whether event
     * start actually relies to the event already running (very important for the camera motion),
     * and to ignore event stops without corresponding start.
     */
    virtual QString sequenceKey() const
    {
        return aggregationKey();
    }

    /**
     * Used for caching and limiting repeat of instant events or repeating 'start' of prolonged
     * events.
     * Returns empty string by default.
     */
    virtual QString cacheKey() const;

    /**
     * Event details (such as caption, description, timestamp, source etc.) in a human-readable
     *     form. These details are used to show the event information in the event log, and able to
     *     be substituted to action fields. Also they are used by default in some built-in actions
     *     like Notification or Email actions to make the actual content.
     * @param context System Context to get all required resources data.
     * @param aggregatedInfo Aggregation info from the database. This info is stored when the
     *     aggregated event is processed, and can be used to show additional details if the only
     *     sample event is accessible (e.g. in the Event Log).
     * @param detailLevel Detail level of the resource name string representation.
     */
    virtual QVariantMap details(
        common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo,
        Qn::ResourceInfoLevel detailLevel) const;

    virtual nx::vms::api::rules::PropertyMap aggregatedInfo(
        const AggregatedEvent& aggregatedEvent) const;

protected:
    BasicEvent() = default;

    /** Event name, e.g. "Device Disconnected". Added to details, used in substitutions . */
    virtual QString name(common::SystemContext* context) const;

    /**
     * Extended event caption, e.g. "MyCamera was disconnected". Added to details, used in some
     * built-in actions, e.g. as email subject or bookmark name.
     */
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const;

    /**
     * Fill all aggregation-related details, using passed id as a server id.
     * @param useAsSource Fill source-related details as well.
     */
    static void fillAggregationDetailsForServer(
        QVariantMap& details,
        common::SystemContext* context,
        nx::Uuid serverId,
        Qn::ResourceInfoLevel detailLevel,
        bool useAsSource = true);

    /**
     * Fill all aggregation-related details, using passed id as a device id.
     * @param useAsSource Fill source-related details as well.
     */
    static void fillAggregationDetailsForDevice(
        QVariantMap& details,
        common::SystemContext* context,
        nx::Uuid deviceId,
        Qn::ResourceInfoLevel detailLevel,
        bool useAsSource = true);

private:
    std::chrono::microseconds m_timestamp = std::chrono::microseconds::zero();
    State m_state = State::none;
};

} // namespace nx::vms::rules
