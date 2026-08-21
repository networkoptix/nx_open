// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

namespace nx::vms::common { struct LayoutItemData; }

namespace nx::vms::client::desktop {

/**
 * Create layout item from resource.
 * @param forceCloud Generates cloud path even for resources from the current system.
 */
nx::vms::common::LayoutItemData layoutItemFromResource(
    const QnResourcePtr& resource, bool forceCloud = false);

/** Create a new layout resource with a given resource on it. */
core::LayoutResourcePtr layoutFromResource(const QnResourcePtr& resource);

/** Get all resources placed on the layout. WARNING: method is SLOW! */
QSet<QnResourcePtr> layoutResources(const QnLayoutResourcePtr& layout);

/** Check whether resource belongs to the given layout. */
bool resourceBelongsToLayout(const QnResourcePtr& resource, const QnLayoutResourcePtr& layout);

/**
 * Convert a common layout to a cloud one. The background image, if any, is copied into the cloud
 * wallpapers cache in the background, so it can be uploaded to the cloud when the layout is saved.
 * If the image is not available locally, the layout background is reset.
 * @param onCompleted Invoked once the background image is copied or the background is reset, so
 *     the converted layout is ready to be saved. Always invoked asynchronously and in the thread
 *     of the converted layout, and not invoked at all if the layout cannot be converted or is
 *     destroyed in the meantime.
 */
core::LayoutResourcePtr convertLayoutToCloud(
    const core::LayoutResourcePtr& layout,
    core::LayoutResource::ItemsRemapHash* itemsRemapHash = nullptr,
    std::function<void(const core::LayoutResourcePtr& cloudLayout)> onCompleted = {});

} // namespace nx::vms::client::desktop
