// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::common {
namespace camera_hotspots {

/**
 * Predicate that determines whether camera hotspots can be set up for the given camera resource
 * or not.
 */
NX_VMS_COMMON_API bool supportsCameraHotspots(const QnResourcePtr& cameraResource);

/**
 * Predicate that determines whether some hotspot can refer to the given camera or layout resource
 * or not.
 */
NX_VMS_COMMON_API bool hotspotCanReferToResource(const QnResourcePtr& targetResource);

} // namespace camera_hotspots
} // namespace nx::vms::common
