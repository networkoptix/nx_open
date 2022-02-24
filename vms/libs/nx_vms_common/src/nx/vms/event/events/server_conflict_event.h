// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <core/resource/resource_fwd.h>

#include <nx/vms/event/events/conflict_event.h>

// TODO: #vkutin Think of a location to put this to
struct NX_VMS_COMMON_API QnCameraConflictList
{
    QString sourceServer;
    QHash<QString, QStringList> camerasByServer;

    QString encode() const;
    void decode(const QString &encoded);
};

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API ServerConflictEvent: public ConflictEvent
{
    using base_type = ConflictEvent;

public:
    explicit ServerConflictEvent(const QnResourcePtr& server, qint64 timeStamp,
        const QnCameraConflictList& conflictList);

    explicit ServerConflictEvent(const QnResourcePtr& server, qint64 timeStamp,
        const nx::vms::api::ModuleInformation& conflictModule, const QUrl& conflictUrl);

private:
    QnCameraConflictList m_cameraConflicts;
};

} // namespace event
} // namespace vms
} // namespace nx
