// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Set of utilities required primarily for displaying Event Log contents.
 */

#include <QtCore/QString>

#include <nx/utils/uuid.h>

#include "../basic_event.h"

namespace nx::vms::rules::utils {

/**
 * Event source icon. Returned in form of resource type and a flag whether plural form of icon
 * should be used.
 */
NX_VMS_RULES_API std::pair<ResourceType, bool /*plural*/> eventSourceIcon(
    const QVariantMap& eventDetails);

/**
 * Event source icon. Returned in form of resource type and a flag whether plural form of icon
 * should be used.
 */
NX_VMS_RULES_API std::pair<ResourceType, bool /*plural*/> eventSourceIcon(
    const AggregatedEventPtr& event,
    common::SystemContext* context);

/**
 * Event source text. By default this is the device (if present) or server name where the event was
 * produced. Generic event may overwrite it.
 */
NX_VMS_RULES_API QString eventSourceText(const QVariantMap& eventDetails);

/**
 * Event source text. By default this is the device (if present) or server name where the event was
 * produced. Generic event may overwrite it.
 */
NX_VMS_RULES_API QString eventSourceText(
    const AggregatedEventPtr& event,
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel);

/**
 * Event source resources. By default this is the device (if present) or server where the event was
 * produced. Some events may have several source resources - e.g. Soft Trigger for the aggregated
 * event may be produced on different cameras. Generic event may have any number of source devices
 * in a single instance. Integration diagnostic event may have engine resource as a source in
 * addition to devices - or only engine.
 */
NX_VMS_RULES_API UuidList eventSourceResourceIds(const QVariantMap& eventDetails);

/**
 * Event source resources. By default this is the device (if present) or server where the event was
 * produced. Some events may have several source resources - e.g. Soft Trigger for the aggregated
 * event may be produced on different cameras. Generic event may have any number of source devices
 * in a single instance. Integration diagnostic event may have engine resource as a source in
 * addition to devices - or only engine.
 */
NX_VMS_RULES_API UuidList eventSourceResourceIds(
    const AggregatedEventPtr& event,
    common::SystemContext* context);

} // namespace nx::vms::rules::utils
