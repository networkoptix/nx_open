#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/condition/condition_types.h>

namespace nx {
namespace client {
namespace desktop {

bool isRadassSupported(const QnLayoutResourcePtr& layout, MatchMode match);
ConditionResult isRadassSupported(const QnLayoutResourcePtr& layout);
bool isRadassSupported(const QnLayoutItemIndex& item);
bool isRadassSupported(const QnLayoutItemIndexList& items, MatchMode match);
ConditionResult isRadassSupported(const QnLayoutItemIndexList& items);

} // namespace desktop
} // namespace client
} // namespace nx
