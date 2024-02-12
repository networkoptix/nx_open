// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx::core::access {

/**
 * An abstract interface of an access subjects watcher that reports their own global permissions
 * and their changes.
 */
class NX_VMS_COMMON_API AbstractGlobalPermissionsWatcher: public QObject
{
    Q_OBJECT

public:
    explicit AbstractGlobalPermissionsWatcher(QObject* parent = nullptr): QObject(parent) {}

    virtual nx::vms::api::GlobalPermissions ownGlobalPermissions(const nx::Uuid& subjectId) const = 0;

signals:
    void ownGlobalPermissionsChanged(const nx::Uuid& subjectId);
    void globalPermissionsReset();
};

} // namespace nx::core::access
