#pragma once

#include <nx/utils/thread/mutex.h>

#include <nx/vms/cloud_integration/connect_to_cloud_watcher.h>

namespace ec2 {

class AbstractTransactionMessageBus;

class CloudConnector:
    public nx::vms::cloud_integration::AbstractEc2CloudConnector
{
public:
    CloudConnector(AbstractTransactionMessageBus* messageBus);

    virtual void startDataSynchronization(
        const std::string& peerId,
        const nx::utils::Url& cloudUrl) override;

    virtual void stopDataSynchronization() override;

private:
    nx::utils::Mutex m_mutex;
    AbstractTransactionMessageBus* m_messageBus;
    std::optional<QnUuid> m_currentPeerId;
};

} // namespace ec2
