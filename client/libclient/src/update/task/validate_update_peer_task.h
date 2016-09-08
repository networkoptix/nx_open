#pragma once

#include <update/task/network_peer_task.h>
#include <api/server_rest_connection_fwd.h>

class QnValidateUpdatePeerTask: public QnNetworkPeerTask
{
    using base_type = QnNetworkPeerTask;

public:
    enum ErrorCode
    {
        NoError = 0,
        ValidationFailed,
        CloudHostConflict
    };

    explicit QnValidateUpdatePeerTask(QObject* parent = nullptr);

protected:
    virtual void doStart() override;
    virtual void doCancel() override;

private:
    void validateCloudHost();
};
