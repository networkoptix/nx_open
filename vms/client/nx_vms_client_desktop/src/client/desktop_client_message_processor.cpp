// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_client_message_processor.h"

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resource_factory.h>

using namespace nx::vms::client::desktop;

QnResourceFactory* QnDesktopClientMessageProcessor::getResourceFactory() const
{
    return appContext()->resourceFactory();
}
