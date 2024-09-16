// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_connection_factory.h"

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/utils/software_version.h>
#include <nx/vms/client/core/application_context.h>

using namespace nx::cloud::db::api;

namespace nx::vms::client::core {

CloudConnectionFactory::CloudConnectionFactory():
    connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
{
}

std::unique_ptr<Connection> CloudConnectionFactory::createConnection() const
{
    return connectionFactory->createConnection();
}

std::string CloudConnectionFactory::clientId()
{
    const std::string clientType =
        appContext()->localPeerType() == nx::vms::api::PeerType::mobileClient
            ? "mobile_client"
            : "desktop_client";
    return nx::format("%1/%2/%3",
        clientType,
        nx::branding::customization(),
        nx::utils::SoftwareVersion(nx::build_info::vmsVersion())
            .toString(nx::utils::SoftwareVersion::Format::minor))
        .toStdString();
}

} // namespace nx::vms::client::core
