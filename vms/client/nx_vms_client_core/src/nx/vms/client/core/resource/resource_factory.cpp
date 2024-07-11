// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_factory.h"

#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <nx_ec/data/api_conversion_functions.h>

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
            if (auto userModel = connection->connectionInfo().compatibilityUserModel)
            {
                NX_ASSERT(
                    connection->moduleInformation().version < nx::utils::SoftwareVersion(6, 0),
                    "Compatibility model should not be requested for the 6.0 systems");

                if (userModel->id == data.id)
                    return QnUserResourcePtr(new UserResource(*userModel));

                // For the pre-5.0 versions we do not know the user id.
                if (!connection->isRestApiSupported()
                    && QString::compare(userModel->name, data.name, Qt::CaseInsensitive) == 0)
                {
                    userModel->id = data.id;
                    return QnUserResourcePtr(new UserResource(*userModel));
                }
            }
        }
    }

    QnUserResourcePtr result(new UserResource(data.type, data.externalId));
    ec2::fromApiToResource(data, result);
    return result;
}

} // namespace nx::vms::client::core
