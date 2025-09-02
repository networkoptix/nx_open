// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mobile_client_message_processor.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/mobile_client_resource_factory.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx_ec/abstract_ec_connection.h>

void QnMobileClientMessageProcessor::updateResource(
    const QnResourcePtr &resource,
    ec2::NotificationSource source)
{
    base_type::updateResource(resource, source);

    const auto context = systemContext()->as<nx::vms::client::core::SystemContext>();
    if (context && resource->getId() == context->currentServerId())
        updateMainServerApiUrl(resource.dynamicCast<QnMediaServerResource>());
}

QnResourceFactory* QnMobileClientMessageProcessor::getResourceFactory() const
{
    return nx::vms::client::mobile::appContext()->resourceFactory();
}

void QnMobileClientMessageProcessor::updateMainServerApiUrl(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return;

    if (!m_connection)
        return;

    server->setPrimaryAddress(m_connection->address());
}
