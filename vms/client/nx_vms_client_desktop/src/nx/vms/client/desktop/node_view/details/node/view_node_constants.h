// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

enum ViewDataProperty
{
    isSeparatorProperty,
    isExpandedProperty,
    isHeaderProperty,
    groupSortOrderProperty,
    headerDataProperty,
    hoverableProperty, //< Items are hoverable by default, but you can turn it off.

    lastNodeViewProperty = 128 //< All other properties should start at least from this value.
};

enum CustomViewNodeRole
{
    firstCustomViewNodeRole = Qt::UserRole,

    progressRole = firstCustomViewNodeRole,
    useSwitchStyleForCheckboxRole,

    useItalicFontRole,

    lastRole,
    lastViewNodeRole = lastRole + 1024,

    userChangesRoleOffset = Qt::UserRole + 10000
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
