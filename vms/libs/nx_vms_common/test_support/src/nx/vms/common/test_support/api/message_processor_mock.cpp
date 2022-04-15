// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_processor_mock.h"

#include "../resource/test_resource_factory.h"

namespace nx::vms::common::test {

MessageProcessorMock::MessageProcessorMock(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(context, parent),
    m_resourceFactory(std::make_unique<TestResourceFactory>())
{
}

MessageProcessorMock::~MessageProcessorMock()
{
}

void MessageProcessorMock::emulateConnectionEstablished()
{
    emit initialResourcesReceived();
}

QnResourceFactory* MessageProcessorMock::getResourceFactory() const
{
    return m_resourceFactory.get();
}

} // namespace nx::vms::common::test
