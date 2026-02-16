// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_camera_data_provider.h"

#include "webrtc_consumer.h"

namespace nx::webrtc {

void AbstractCameraDataProvider::setDataConsumer(Consumer* consumer)
{
    m_consumer = consumer;
}

} // namespace nx::webrtc
