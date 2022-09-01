// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/condition/condition_types.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

bool isRadassSupported(const QnLayoutResourcePtr& layout, MatchMode match);
ConditionResult isRadassSupported(const QnLayoutResourcePtr& layout);
bool isRadassSupported(const LayoutItemIndex& item);
bool isRadassSupported(const QnVirtualCameraResourcePtr& camera);
bool isRadassSupported(const QnVirtualCameraResourceList& cameras, MatchMode match);
bool isRadassSupported(const LayoutItemIndexList& items, MatchMode match);
ConditionResult isRadassSupported(const LayoutItemIndexList& items);

} // namespace nx::vms::client::desktop
