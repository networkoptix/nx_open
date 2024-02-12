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

/** Represent some period of time. */
struct TimePeriod
{
    std::chrono::milliseconds startTimeMs;
    std::chrono::milliseconds durationMs; //< '-1' means infinite duration.
};
NX_REFLECTION_INSTRUMENT(TimePeriod, (startTimeMs)(durationMs))

/**
 * Represent media parameters for the workbench item. Media parameters exist only for the
 * items with available video streams.
 */
struct MediaParams
{
    std::optional<double> speed = 1.0;
    std::optional<std::chrono::milliseconds> timestampMs;
    std::optional<TimePeriod> timelineSelection;
    std::optional<TimePeriod> timelineWindow;
};
NX_REFLECTION_INSTRUMENT(MediaParams, (speed)(timestampMs)(timelineSelection)(timelineWindow))

/**
 * Represent item parameters. Media parameters are optional and available only for
 * the appropriate item types.
 */
struct ItemParams
{
    std::optional<bool> selected;
    std::optional<bool> focused;
    std::optional<Rect> geometry;

    std::optional<MediaParams> media;
};
NX_REFLECTION_INSTRUMENT(ItemParams,(selected)(focused)(geometry)(media))

/** Represent workbench item. Consist of the bound resource and parameters of the item. */
struct Item
{
    nx::Uuid id;

    Resource resource;
    ItemParams params;
};
NX_REFLECTION_INSTRUMENT(Item, (id)(resource)(params))

/** Represent result of an operation with an item like item addition or its request. */
struct ItemResult
{
    Error error;
    std::optional<Item> item;
};
NX_REFLECTION_INSTRUMENT(ItemResult, (error)(item))

using ItemVector = std::vector<Item>;

/**
 * Represent items selection state. Consist of the focused item and array of the items
 * which are selected.
 */
struct ItemSelection
{
    std::optional<Item> focusedItem;
    std::optional<ItemVector> selectedItems;
};
NX_REFLECTION_INSTRUMENT(ItemSelection, (focusedItem)(selectedItems))

/** Represent currently supported properties of the underlying layout. */
struct LayoutProperties
{
    std::optional<Size> minimumSize;
    std::optional<bool> locked;
};
NX_REFLECTION_INSTRUMENT(LayoutProperties, (minimumSize)(locked))

/** Represent full state of the tab. */
struct State
{
    bool sync = false;

    ItemVector items;
    ItemSelection selection;

    LayoutProperties properties;
};
NX_REFLECTION_INSTRUMENT(State, (sync)(items)(selection)(properties))

} // namespace nx::vms::client::desktop::jsapi::detail
