// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

struct QnLayoutItemData;

namespace nx::vms::client::desktop::layout {

/** Extract resource path for storing in layout item. */
QString resourcePath(const QnResourcePtr& resource);

/** Implements common client logic for creating layout item from resource. */
QnLayoutItemData itemFromResource(const QnResourcePtr& resource);

/** Create a new layout resource with a given resource on it. */
QnLayoutResourcePtr layoutFromResource(const QnResourcePtr& resource);

} // namespace nx::vms::client::desktop::layout
