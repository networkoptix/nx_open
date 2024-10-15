// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QStringList>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>

namespace nx::vms::rules {

struct NX_VMS_COMMON_API CameraConflictList
{
    QString sourceServer;
    QMap<QString, QStringList> camerasByServer;

    QString encode() const;
    void decode(const QString &encoded);

    bool operator==(const CameraConflictList&) const = default;
};

#define CameraConflictList_Fields (sourceServer)(camerasByServer)
QN_FUSION_DECLARE_FUNCTIONS(CameraConflictList, (json), NX_VMS_COMMON_API);

} // namespace nx::vms::rules
