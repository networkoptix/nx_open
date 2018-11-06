#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/condition/condition_types.h>

namespace nx::vms::client::desktop {

bool isRadassSupported(const QnLayoutResourcePtr& layout, MatchMode match);
ConditionResult isRadassSupported(const QnLayoutResourcePtr& layout);
bool isRadassSupported(const QnLayoutItemIndex& item);
bool isRadassSupported(const QnVirtualCameraResourcePtr& camera);
bool isRadassSupported(const QnVirtualCameraResourceList& cameras, MatchMode match);
bool isRadassSupported(const QnLayoutItemIndexList& items, MatchMode match);
ConditionResult isRadassSupported(const QnLayoutItemIndexList& items);

} // namespace nx::vms::client::desktop
