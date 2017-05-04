#pragma once

#include <core/resource_access/resource_access_subject.h>
#include <core/resource/resource_fwd.h>

#include <common/common_globals.h>

class QnAbstractPermissionsModel
{
public:
    virtual Qn::GlobalPermissions rawPermissions() const = 0;
    virtual void setRawPermissions(Qn::GlobalPermissions value) = 0;

    virtual QSet<QnUuid> accessibleResources() const = 0;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) = 0;

    virtual QnResourceAccessSubject subject() const = 0;
};
