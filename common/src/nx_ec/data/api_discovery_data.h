#pragma once

#include "api_data.h"
#include <network/module_information.h>

namespace ec2 {

    struct ApiDiscoveryData : ApiIdData 
    {
        ApiDiscoveryData(): ignore(false) {}

        QString url;
        bool ignore;
    };

#define ApiDiscoveryData_Fields ApiIdData_Fields(url)(ignore)(id)

    struct ApiDiscoverPeerData : ApiData
    {
        QString url;
    };
#define ApiDiscoverPeerData_Fields (url)

    struct ApiDiscoveredServerData : QnModuleInformationWithAddresses
    {
        ApiDiscoveredServerData() : status(Qn::Online) {}
        ApiDiscoveredServerData(const QnModuleInformation &other) :
            QnModuleInformationWithAddresses(other),
            status(Qn::Online)
        {}
        // Should be only Online, Incompatible or Unauthorized
        Qn::ResourceStatus status;
    };
#define ApiDiscoveredServerData_Fields QnModuleInformationWithAddresses_Fields(status)

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiDiscoveryData);
Q_DECLARE_METATYPE(ec2::ApiDiscoveredServerData);
