// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "geometry_structures.h"
#include "globals_structures.h"
#include "resources_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {

/**
 * Represents some period of time.
 * @ingroup vms
 */
struct TimePeriod
{
    /** Start time in milliseconds from Epoch. */
    std::chrono::milliseconds startTimeMs;

    /** Duration in milliseconds, -1 means infinite duration. */
    std::chrono::milliseconds durationMs;
};
NX_REFLECTION_INSTRUMENT(TimePeriod, (startTimeMs)(durationMs))

/**
 * Represents media parameters for a tab item. Media parameters exist only for items with available
 * video streams.
 * @ingroup vms
 */
struct MediaParams
{
    /** Playback speed. */
    std::optional<double> speed = 1.0;

    /** Current timestamp of the media stream. */
    std::optional<std::chrono::milliseconds> timestampMs;

    /** Selected timeline part. */
    std::optional<TimePeriod> timelineSelection;

    /** Visible part of the timeline. */
    std::optional<TimePeriod> timelineWindow;
};
NX_REFLECTION_INSTRUMENT(MediaParams, (speed)(timestampMs)(timelineSelection)(timelineWindow))

/**
 * Represents tab item parameters. Media parameters are optional and available only for the
 * appropriate item types.
 * @ingroup vms
 */
struct ItemParams
{
    /** Shows if the item is in the selection or is single-selected. */
    std::optional<bool> selected;

    /** Shows if the item is a focused one. */
    std::optional<bool> focused;

    /** Geometry of the item. */
    std::optional<Rect> geometry;

    /** Media parameters of the item (if applicable). */
    std::optional<MediaParams> media;
};
NX_REFLECTION_INSTRUMENT(ItemParams,(selected)(focused)(geometry)(media))

/**
 * Represents the tab item description.
 * @ingroup vms
 */
struct Item
{
    /** Unique item identifier. */
    QnUuid id;

    /** Resource connected to the item. */
    Resource resource;

    /** Item parameters related to the appearance. */
    ItemParams params;

};
NX_REFLECTION_INSTRUMENT(Item, (id)(resource)(params))

/**
 * Represents item operation result.
 * @ingroup vms
 */
struct ItemResult
{
    /** Error description. */
    Error error;

    /** Related item description. */
    std::optional<Item> item;
};
NX_REFLECTION_INSTRUMENT(ItemResult, (error)(item))

using ItemVector = std::vector<Item>;

/**
 * Represents selection and focus states for the tab.
 * @ingroup vms
 */
struct ItemSelection
{
    /** Currently focused item on the tab. */
    std::optional<Item> focusedItem;

    /** Array of selected items on the tab. */
    std::optional<ItemVector> selectedItems;
};
NX_REFLECTION_INSTRUMENT(ItemSelection, (focusedItem)(selectedItems))

/**
 * Represents all properties of the current Layout on the tab, that are available to the API.
 * @ingroup vms
 */
struct LayoutProperties
{
    /** Minimum size of the Layout, if specified. */
    std::optional<Size> minimumSize;

    /** The state "locked". */
    std::optional<bool> locked;
};
NX_REFLECTION_INSTRUMENT(LayoutProperties, (minimumSize)(locked))

/**
 * Represents the complete current state of the tab.
 * @ingroup vms
 */
struct State
{
    /** Shows if the current Layout on the tab is synced. */
    bool sync = false;

    /** Array of the all items on the tab. */
    ItemVector items;

    /** Current selection state. */
    ItemSelection selection;

    /** Related Layout properties. */
    LayoutProperties properties;
};
NX_REFLECTION_INSTRUMENT(State, (sync)(items)(selection)(properties))

} // namespace nx::vms::client::desktop::jsapi::detail
