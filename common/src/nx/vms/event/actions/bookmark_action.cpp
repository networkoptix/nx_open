#include "bookmark_action.h"

namespace nx {
namespace vms {
namespace event {

BookmarkAction::BookmarkAction(const EventParameters& runtimeParams):
    base_type(event::BookmarkAction, runtimeParams)
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
