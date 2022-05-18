// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>

namespace nx::vms::rules {

struct NX_VMS_COMMON_API CameraConflictList
{
    QString sourceServer;
    QHash<QString, QStringList> camerasByServer;

    QString encode() const;
    void decode(const QString &encoded);
};

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(nx::vms::rules::CameraConflictList);
