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
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    if (resource->flags().testFlag(Qn::server))
        return true;

    return false;
}
