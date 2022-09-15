// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

class QnWorkbenchAccessController;

namespace nx::vms::client::desktop {

/**
 * Unified interface to check resource access permissions for all System Contexts.
 */
class ResourceAccessManager
{
public:
    /** Full list of permissions for the given resource. */
    static Qn::Permissions permissions(const QnResourcePtr& resource);

    /** Check whether user has all required permissions for the given resource. */
    static bool hasPermissions(const QnResourcePtr& resource, Qn::Permissions requiredPermissions);

    /** Access controller of the resource's System Context (if any), nullptr otherwise. */
    static QnWorkbenchAccessController* accessController(const QnResourcePtr& resource);
};

} // namespace nx::vms::client::desktop
