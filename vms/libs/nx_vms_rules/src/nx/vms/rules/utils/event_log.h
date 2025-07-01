// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Set of utilities required primarily for displaying Event Log contents.
 */

#include <QtCore/QString>

#include <nx/utils/uuid.h>

#include "../basic_event.h"

namespace nx::vms::rules::utils {

struct NX_VMS_RULES_API EventLog
{
    /**
     * Event source icon. Returned in form of resource type and a flag whether plural form of icon
     * should be used.
     */
    static std::pair<ResourceType, bool /*plural*/> sourceIcon(const QVariantMap& eventDetails);

    /**
     * Event source icon. Returned in form of resource type and a flag whether plural form of icon
     * should be used.
     */
    static std::pair<ResourceType, bool /*plural*/> sourceIcon(
        const AggregatedEventPtr& event, common::SystemContext* context);

    /**
     * Event source text. By default this is the device (if present) or server name where the event
     * was produced. Generic event may overwrite it.
     */
    static QString sourceText(common::SystemContext* context, const QVariantMap& eventDetails);

    /**
     * Event source text. By default this is the device (if present) or server name where the event
     * was produced. Generic event may overwrite it.
     */
    static QString sourceText(const AggregatedEventPtr& event,
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel);

    /**
     * Event source resources. By default this is the device (if present) or server where the event
     * was produced. Some events may have several source resources - e.g. Soft Trigger for the
     * aggregated event may be produced on different cameras. Generic event may have any number of
     * source devices in a single instance. Integration diagnostic event may have engine resource
     * as a source in addition to devices - or only engine.
     */
    static UuidList sourceResourceIds(const QVariantMap& eventDetails);

    /**
     * Event source resources. By default this is the device (if present) or server where the event
     * was produced. Some events may have several source resources - e.g. Soft Trigger for the
     * aggregated event may be produced on different cameras. Generic event may have any number of
     * source devices in a single instance. Integration diagnostic event may have engine resource
     * as a source in addition to devices - or only engine.
     */
    static UuidList sourceResourceIds(
        const AggregatedEventPtr& event, common::SystemContext* context);

    /**
     * Event description tooltip text. By default it contains the extended event description.
     */
    static QString descriptionTooltip(const AggregatedEventPtr& event,
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel);
};

} // namespace nx::vms::rules::utils
