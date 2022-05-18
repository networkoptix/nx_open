// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/events/conflict_event.h>
#include <nx/vms/rules/camera_conflict_list.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API ServerConflictEvent: public ConflictEvent
{
    using base_type = ConflictEvent;

public:
    explicit ServerConflictEvent(
        const QnResourcePtr& server,
        qint64 timeStamp,
        const nx::vms::rules::CameraConflictList& conflictList);

private:
    nx::vms::rules::CameraConflictList m_cameraConflicts;
};

} // namespace nx::vms::event
