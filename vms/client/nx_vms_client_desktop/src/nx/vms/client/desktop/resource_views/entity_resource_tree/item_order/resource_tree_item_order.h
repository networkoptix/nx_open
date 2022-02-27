// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

/**
 * Provides alphanumeric case insensitive order and grouping:
 * 1) Any Servers except listed below.
 * 2) Incompatible Servers.
 */
entity_item_model::ItemOrder serversOrder();

/**
 * Provides alphanumeric case insensitive order and grouping:
 * 1) Shared Layouts.
 * 2) User owned Layouts.
 */
entity_item_model::ItemOrder layoutsOrder();

/**
 * Provides alphanumeric case insensitive order and grouping:
 * 1) Any devices, except listed below
 * 2) Health monitors
 * 3) Proxied Web Resources
 * 4) Web Pages
 */
entity_item_model::ItemOrder layoutItemsOrder();

/**
 * Provides alphanumeric case insensitive order and grouping:
 * 1) File layouts
 * 2) Video files
 * 3) Image files
 */
entity_item_model::ItemOrder locaResourcesOrder();

/**
 * Provides alphanumeric case insensitive order and grouping:
 * 1) All devices
 * 2) Proxied Web Resource
 */
entity_item_model::ItemOrder serverResourcesOrder();

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
