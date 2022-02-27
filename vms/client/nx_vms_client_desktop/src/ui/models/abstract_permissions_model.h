// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/resource_access_subject.h>
#include <core/resource/resource_fwd.h>

#include <common/common_globals.h>

class QnAbstractPermissionsModel
{
public:
    virtual GlobalPermissions rawPermissions() const = 0;
    virtual void setRawPermissions(GlobalPermissions value) = 0;

    virtual QSet<QnUuid> accessibleResources() const = 0;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) = 0;

    virtual QnResourceAccessSubject subject() const = 0;
};
