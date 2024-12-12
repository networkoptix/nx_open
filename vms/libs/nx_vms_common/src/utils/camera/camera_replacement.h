// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::common {
namespace utils {
namespace camera_replacement {

// TODO: Use QnVirtualCameraResourcePtr for the argument.
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
 *     - It's device that provides video output, e.g. not a speaker
 */
bool NX_VMS_COMMON_API cameraSupportsReplacement(const QnResourcePtr& resource);

// TODO: Use QnVirtualCameraResourcePtr for the argument.
/**
 * Predicate which determines if camera can actually be replaced with another one right now.
 * @param resource Valid pointer to the camera resource expected.
 * @return True if given camera supports replacement (see <tt>cameraSupportsReplacement()</tt>)
 *     and also has offline status while its parent server has online status.
 */
bool NX_VMS_COMMON_API cameraCanBeReplaced(const QnResourcePtr& resource);

// TODO: Use QnVirtualCameraResourcePtr for the argument.
/**
 * Predicate which determines if camera can actually be used as a replacement for another one
 * right now.
 * @param cameraToBeReplaced Valid pointer to the camera resource expected.
 * @param replacementCamera Valid pointer to the camera resource expected.
 * @return True if given camera supports replacement (see <tt>cameraSupportsReplacement()</tt>),
 *     has online status and has same parent server as camera to be replaced.
 */
bool NX_VMS_COMMON_API cameraCanBeUsedAsReplacement(
    const QnResourcePtr& cameraToBeReplaced,
    const QnResourcePtr& replacementCamera);

// TODO: Use QnVirtualCameraResourcePtr for the argument.
/**
 * @param resource Valid pointer to the camera resource expected.
 * @return True if given camera is used as replacement one for some another camera.
 */
bool NX_VMS_COMMON_API isReplacedCamera(const QnResourcePtr& resource);

} // namespace camera_replacement
} // namespace utils
} // namespace nx::vms::common
