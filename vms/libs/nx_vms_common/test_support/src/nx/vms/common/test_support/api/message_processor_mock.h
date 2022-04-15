// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/common_message_processor.h>

namespace nx::vms::common::test {

class TestResourceFactory;

class NX_VMS_COMMON_TEST_SUPPORT_API MessageProcessorMock: public QnCommonMessageProcessor
{
    using base_type = QnCommonMessageProcessor;

public:
    MessageProcessorMock(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    virtual ~MessageProcessorMock() override;

    void emulateConnectionEstablished();

protected:
    virtual void onResourceStatusChanged(
        const QnResourcePtr &resource,
        nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source) override
    {
    };

    virtual QnResourceFactory* getResourceFactory() const override;

private:
    std::unique_ptr<TestResourceFactory> m_resourceFactory;
};

} // namespace nx::vms::common::test
