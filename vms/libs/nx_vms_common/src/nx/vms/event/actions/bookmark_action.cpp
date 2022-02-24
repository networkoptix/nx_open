// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_action.h"

namespace nx {
namespace vms {
namespace event {

BookmarkAction::BookmarkAction(const EventParameters& runtimeParams):
    base_type(ActionType::bookmarkAction, runtimeParams)
{
}

QString BookmarkAction::getExternalUniqKey() const
{
    return lit("%1%2")
        .arg(base_type::getExternalUniqKey())
        .arg(getRuleId().toString());
}

} // namespace event
} // namespace vms
} // namespace nx
