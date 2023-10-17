// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <client/client_message_processor.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class SystemContext;

namespace test {

class MessageProcessorMock: public QnClientMessageProcessor
{
    using base_type = QnClientMessageProcessor;

public:
    MessageProcessorMock(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    virtual ~MessageProcessorMock() override;

    void emulateConnectionEstablished();

protected:
    virtual void onResourceStatusChanged(
        const QnResourcePtr&,
        nx::vms::api::ResourceStatus,
        ec2::NotificationSource) override
    {
    };

    virtual QnResourceFactory* getResourceFactory() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace test
} // namespace nx::vms::client::desktop
