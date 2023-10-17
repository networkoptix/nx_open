// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_processor_mock.h"

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/test_support/resource/test_resource_factory.h>

namespace nx::vms::client::desktop::test {

struct MessageProcessorMock::Private
{
    std::unique_ptr<common::test::TestResourceFactory> resourceFactory =
        std::make_unique<common::test::TestResourceFactory>();
};

MessageProcessorMock::MessageProcessorMock(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(context, parent),
    d(new Private())
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
    return d->resourceFactory.get();
}

} // namespace nx::vms::client::desktop::test
