// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::desktop {

enum class ResourceSelectionMode
{
    MultiSelection, //< Arbitrary selection, may be null. Groups are checkable.
    SingleSelection, //< Single resource or none.
    ExclusiveSelection, //< Exactly one resource, radio group style appearance.
};

} // namespace nx::vms::client::desktop
