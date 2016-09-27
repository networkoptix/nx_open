#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_provider.h>

class QnAbstractPermissionsModel
{
public:
    virtual Qn::GlobalPermissions rawPermissions() const = 0;
    virtual void setRawPermissions(Qn::GlobalPermissions value) = 0;

    virtual QSet<QnUuid> accessibleResources() const = 0;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) = 0;

    /* Layouts accessible indirectly (with a set of access providers) or directly: */
    virtual QnIndirectAccessProviders accessibleLayouts() const = 0;
};
