// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_factory.h"

#include "media_server_resource.h"

QnMediaServerResourcePtr QnResourceFactory::createServer() const
{
    return QnMediaServerResourcePtr(new QnMediaServerResource());
}
