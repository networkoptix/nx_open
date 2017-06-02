#pragma once

#include <nx/vms/event/actions/common_action.h>

namespace nx {
namespace vms {
namespace event {

class BookmarkAction: public CommonAction
{
    using base_type = CommonAction;

public:
    BookmarkAction(const EventParameters& runtimeParams);
    virtual QString getExternalUniqKey() const override;
};

} // namespace event
} // namespace vms
} // namespace nx
