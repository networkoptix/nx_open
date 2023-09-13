// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>

namespace nx::vms::common {

// Calculates is there intercom call port on camera.
NX_VMS_COMMON_API bool checkIntercomCallPortExists(const QnVirtualCameraResourcePtr& camera);

// Calculates intercom child layout id. It is unique for the intercom.
NX_VMS_COMMON_API QnUuid calculateIntercomLayoutId(const QnResourcePtr& intercom);

NX_VMS_COMMON_API QnUuid calculateIntercomLayoutId(const QnUuid& intercomId);

// Checks if the resource is an intercom.
// WARNING: It can return false negative result if an intercom is in removing process.
// Because any intercom-specific feature could be already removed from resource.
NX_VMS_COMMON_API bool isIntercom(const QnResourcePtr& resource);

// Checks if the resource is an intercom placed on it's own child layout.
// WARNING: It can return false negative result if an intercom is in removing process.
// Because any intercom-specific feature could be already removed from resource.
NX_VMS_COMMON_API bool isIntercomOnIntercomLayout(
    const QnResourcePtr& resource,
    const QnLayoutResourcePtr& layoutResource);

// Checks if the resource is an intercom's child layout.
// WARNING: It can return false negative result if an intercom is in removing process.
// Because any intercom-specific feature could be already removed from resource.
NX_VMS_COMMON_API bool isIntercomLayout(const QnResourcePtr& layoutResource);

} // namespace nx::vms::common
