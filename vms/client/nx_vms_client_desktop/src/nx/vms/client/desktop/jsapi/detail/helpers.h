// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>

#include "resources_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {

/**
 * Checks if a resource with the specified type may have media stream (at least theoretically).
 */
bool hasMediaStream(const ResourceType type);

ResourceType resourceType(const QnResourcePtr& resource);

/**
 * Checks if resource is currently supported and the user has required permissions.
 */
bool isResourceAvailable(const QnResourcePtr& resource);

QnResourcePtr getResourceIfAvailable(const ResourceUniqueId& resourceId);

} // namespace nx::vms::client::desktop::jsapi::detail
