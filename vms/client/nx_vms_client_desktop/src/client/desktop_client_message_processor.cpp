// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_client_message_processor.h"

#include <core/resource/client_camera_factory.h>

QnResourceFactory* QnDesktopClientMessageProcessor::getResourceFactory() const
{
    return QnClientResourceFactory::instance();
}
