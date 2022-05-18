// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_conflict_event.h"

#include <core/resource/resource.h>

namespace nx::vms::event {

ServerConflictEvent::ServerConflictEvent(
    const QnResourcePtr& server,
    qint64 timeStamp,
    const nx::vms::rules::CameraConflictList& conflictList)
    :
    base_type(EventType::serverConflictEvent, server, timeStamp),
    m_cameraConflicts(conflictList)
{
    m_caption = m_cameraConflicts.sourceServer;
    m_description = m_cameraConflicts.encode();
}

} // namespace nx::vms::event
