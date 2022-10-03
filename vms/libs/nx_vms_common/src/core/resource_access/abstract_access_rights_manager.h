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
    virtual ResourceAccessMap ownResourceAccessMap(const QnUuid& subjectId) const = 0;

    /** Returns specified subject's own access rights to the specified resource. */
    nx::vms::api::AccessRights ownAccessRights(
        const QnUuid& subjectId, const QnUuid& targetId) const;

signals:
    /** Emitted when subjects' access rights map is changed. */
    void ownAccessRightsChanged(const QSet<QnUuid>& subjectIds);

    /** Emitted when all subjects' access maps are reset. */
    void accessRightsReset();

public:
    static const QnUuid kAnyResourceId;
};

} // namespace nx::core::access
