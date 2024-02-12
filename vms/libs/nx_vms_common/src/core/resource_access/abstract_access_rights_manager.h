// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource_access/resource_access_map.h>

namespace nx::core::access {

class NX_VMS_COMMON_API AbstractAccessRightsManager: public QObject
{
    Q_OBJECT

public:
    explicit AbstractAccessRightsManager(QObject* parent = nullptr): QObject(parent) {}

    /** Returns specified subject's own access rights map. */
    virtual ResourceAccessMap ownResourceAccessMap(const nx::Uuid& subjectId) const = 0;

    /** Returns specified subject's own access rights to the specified resource. */
    nx::vms::api::AccessRights ownAccessRights(
        const nx::Uuid& subjectId, const nx::Uuid& targetId) const;

signals:
    /** Emitted when subjects' access rights map is changed. */
    void ownAccessRightsChanged(const QSet<nx::Uuid>& subjectIds);

    /** Emitted when all subjects' access maps are reset. */
    void accessRightsReset();
};

} // namespace nx::core::access
