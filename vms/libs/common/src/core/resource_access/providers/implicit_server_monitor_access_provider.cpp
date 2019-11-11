#include "implicit_server_monitor_access_provider.h"

#include <core/resource/resource.h>

using namespace nx::core::access;

QnImplicitServerMonitorAccessProvider::QnImplicitServerMonitorAccessProvider(
    Mode mode,
    QObject* parent)
    :
    base_type(mode, parent)
{
}

QnImplicitServerMonitorAccessProvider::~QnImplicitServerMonitorAccessProvider()
{
}

QnAbstractResourceAccessProvider::Source QnImplicitServerMonitorAccessProvider::baseSource() const
{
    return Source::implicitMonitorAccess;
}

bool QnImplicitServerMonitorAccessProvider::calculateAccess(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    nx::vms::api::GlobalPermissions globalPermissions) const
{
    if (!NX_ASSERT(acceptable(subject, resource)))
        return false;

    return resource->hasFlags(Qn::server);
}
