#pragma once

#include <nx/vms/cloud_integration/connect_to_cloud_watcher.h>

namespace ec2 {

class AbstractTransactionMessageBus;

class CloudConnector:
    public nx::vms::cloud_integration::AbstractEc2CloudConnector
{
public:
    CloudConnector(AbstractTransactionMessageBus* messageBus);

    virtual void startDataSynchronization(const QUrl& cloudUrl) override;
    virtual void stopDataSynchronization() override;

private:
    AbstractTransactionMessageBus* m_messageBus;
};

} // namespace ec2
