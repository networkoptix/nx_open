// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_factory.h"

#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>

#include "server.h"
#include "user.h"

namespace nx::vms::client::core {

QnMediaServerResourcePtr ResourceFactory::createServer() const
{
    return QnMediaServerResourcePtr(new ServerResource());
}

QnUserResourcePtr ResourceFactory::createUser(
    nx::vms::common::SystemContext* systemContext,
    const nx::vms::api::UserData& data) const
{
    auto clientSystemContext = dynamic_cast<SystemContext*>(systemContext);
    if (NX_ASSERT(clientSystemContext))
    {
        // On the client side due to queued connections slot may be handled after disconnect.
        const auto connection = clientSystemContext->connection();
        if (connection)
        {
            // User info for the pre-6.0 Systems where old permissions model is implemented.
            const auto userModel = connection->connectionInfo().compatibilityUserModel;
            if (userModel && userModel->id == data.id)
            {
                NX_ASSERT(
                    connection->moduleInformation().version < nx::utils::SoftwareVersion(6, 0),
                    "Compatibility model should not be requested for the 6.0 systems");

                return QnUserResourcePtr(new UserResource(*userModel));
            }
        }
    }

    return base_type::createUser(systemContext, data);
}

} // namespace nx::vms::client::core
