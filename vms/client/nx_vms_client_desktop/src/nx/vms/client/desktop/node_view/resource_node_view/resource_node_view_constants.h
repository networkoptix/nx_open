// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../details/node/view_node_constants.h"

namespace nx::vms::client::desktop {
namespace node_view {

enum ResourceNodeDataRole
{
    resourceColumnRole = details::lastViewNodeRole,
    resourceRole,
    resourceExtraTextRole,
    resourceExtraStatusRole,
};

enum ResourceNodeViewProperty
{
    validResourceProperty = details::lastNodeViewProperty,

    lastResourceNodeViewProperty = details::lastNodeViewProperty + 128
};

} // namespace node_view
} // namespace nx::vms::client::desktop
