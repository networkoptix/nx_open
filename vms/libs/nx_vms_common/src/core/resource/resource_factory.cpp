// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_factory.h"

#include <nx_ec/data/api_conversion_functions.h>

#include "layout_resource.h"
#include "media_server_resource.h"
#include "user_resource.h"

QnMediaServerResourcePtr QnResourceFactory::createServer() const
{
    return QnMediaServerResourcePtr(new QnMediaServerResource());
}

QnLayoutResourcePtr QnResourceFactory::createLayout() const
{
    return QnLayoutResourcePtr(new QnLayoutResource());
}

QnUserResourcePtr QnResourceFactory::createUser(
    nx::vms::common::SystemContext* /*systemContext*/,
    const nx::vms::api::UserData& data) const
{
    QnUserResourcePtr result(new QnUserResource(data.type, data.externalId));
    ec2::fromApiToResource(data, result);
    return result;
}
