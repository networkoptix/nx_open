// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
