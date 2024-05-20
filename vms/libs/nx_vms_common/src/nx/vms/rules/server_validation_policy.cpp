// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_validation_policy.h"

#include <core/resource/media_server_resource.h>

bool QnBuzzerPolicy::isServerValid(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    using namespace nx::vms::api;
    return server->getServerFlags().testFlag(ServerFlag::SF_HasBuzzer);
}

QString QnBuzzerPolicy::infoText()
{
    return tr("Servers that support buzzer");
}

bool QnPoeOverBudgetPolicy::isServerValid(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    using namespace nx::vms::api;
    return server->getServerFlags().testFlag(ServerFlag::SF_HasPoeManagementCapability);
}

QString QnPoeOverBudgetPolicy::infoText()
{
    return tr("Servers that support PoE monitoring");
}

bool QnFanErrorPolicy::isServerValid(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    using namespace nx::vms::api;
    return server->getServerFlags().testFlag(ServerFlag::SF_HasFanMonitoringCapability);
}

QString QnFanErrorPolicy::infoText()
{
    return tr("Servers that support fan diagnostic");
}
