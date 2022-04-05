// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::vms::common {
namespace utils {
namespace edge_device {

/**
 * @param edgeServer Pointer to the <tt>QnMediaServerResource</tt> instance that provides
 *     <tt>nx::vms::api::SF_Edge</tt> server flag is expected.
 * @param camera Pointer to the camera resource is expected.
 * @return True if given camera resource represents a camera that is a part of EDGE device that
 *      running server represented by the given server resource. The camera resource does not have
 *      to be child of the server resource.
 */
bool NX_VMS_COMMON_API isCoupledEdgeCamera(
    const QnMediaServerResourcePtr& edgeServer,
    const QnVirtualCameraResourcePtr& camera);

} // namespace edge_device
} // namespace utils
} // namespace nx::vms::common
