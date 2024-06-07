// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_update_settings.h"

#include <nx/branding.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

ClientUpdateSettings::ClientUpdateSettings():
    enabled(nx::branding::clientAutoUpdateEnabledByDefault()
        && !nx::branding::isDesktopClientCustomized())
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ClientUpdateSettings, (json), ClientUpdateSettings_Fields)

QByteArray ClientUpdateSettings::toString() const
{
    return QJson::serialized(this);
}

} // namespace nx::vms::api
