// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/actions/abstract_action.h>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API RecordingAction: public AbstractAction
{
    using base_type = AbstractAction;

public:
    explicit RecordingAction(const EventParameters& runtimeParams);

    int getFps() const;
    Qn::StreamQuality getStreamQuality() const;
    int getDurationSec() const;
    int getRecordAfterSec() const;
    int getRecordBeforeSec() const;
};

} // namespace event
} // namespace vms
} // namespace nx
