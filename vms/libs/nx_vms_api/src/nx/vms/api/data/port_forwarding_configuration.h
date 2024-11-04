// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx::vms::api {

/**%apidoc Port forwarding configuration of a service on a Server. */
struct NX_VMS_API PortForwardingConfiguration
{
    /**%apidoc User specified name of a service. */
    QString name;

    /**%apidoc
     * Local port of a service on a Server.
     * If not present then the service is not forwarded by a Server and has its own connection.
     */
    std::optional<int> port;

    /**%apidoc[opt] */
    QString login;

    /**%apidoc[opt] */
    QString password;

    bool operator==(const PortForwardingConfiguration&) const = default;
};
#define PortForwardingConfiguration_Fields (name)(port)(login)(password)
QN_FUSION_DECLARE_FUNCTIONS(PortForwardingConfiguration, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PortForwardingConfiguration, PortForwardingConfiguration_Fields);

} // namespace nx::vms::api
