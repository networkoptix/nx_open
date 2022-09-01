// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

struct QnLayoutItemData;

namespace nx::vms::client::desktop {

/** Implements common client logic for creating layout item from resource. */
QnLayoutItemData layoutItemFromResource(const QnResourcePtr& resource);

/** Create a new layout resource with a given resource on it. */
LayoutResourcePtr layoutFromResource(const QnResourcePtr& resource);

/** Get all resources placed on the layout. WARNING: method is SLOW! */
QSet<QnResourcePtr> layoutResources(const QnLayoutResourcePtr& layout);

/** Check whether resource belongs to the given layout. */
bool resourceBelongsToLayout(const QnResourcePtr& resource, const QnLayoutResourcePtr& layout);

} // namespace nx::vms::client::desktop
