// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::common {
namespace utils {
namespace camera_replacement {

/**
 * Predicate that determines if camera has capability of being replaced or vice versa if camera can
 * act as replacement device. Having this capability for both of devices involved is a necessary
 * but not sufficient condition to have ability to perform replacement operation.
 * @param resource Valid pointer to the camera resource expected.
 * @return True if given resource is valid camera resource and following conditions are met:
 *     - Camera is present in the system and represents actual server-based device
 *     - It's not a Virtual Camera
 *     - It's not an archive-based camera obtained by archive reindexing
 *     - It's not an NVR-based camera
 *     - It's not a multisensor camera
 *     - It's not an I/O module
 */
bool NX_VMS_COMMON_API cameraSupportsReplacement(const QnResourcePtr& resource);

/**
 * Predicate which determines if camera can actually be replaced with another one right now.
 * @param resource Valid pointer to the camera resource expected.
 * @return True if given camera supports replacement (see <tt>cameraSupportsReplacement()</tt>)
 *     and also has offline status while its parent server has online status.
 */
bool NX_VMS_COMMON_API cameraCanBeReplaced(const QnResourcePtr& resource);

/**
 * Predicate which determines if camera can actually be used as a replacement for another one
 * right now.
 * @param resource Valid pointer to the camera resource expected.
 * @return True if given camera supports replacement (see <tt>cameraSupportsReplacement()</tt>)
 *     and also has online status.
 */
bool NX_VMS_COMMON_API cameraCanBeUsedAsReplacement(const QnResourcePtr& resource);

} // namespace camera_replacement
} // namespace utils
} // namespace nx::vms::common
