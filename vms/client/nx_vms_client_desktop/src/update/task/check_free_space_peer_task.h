#pragma once

#include <update/task/network_peer_task.h>
#include <api/server_rest_connection_fwd.h>

#include <nx/vms/api/data/system_information.h>

class QnCheckFreeSpacePeerTask: public QnNetworkPeerTask
{
    using base_type = QnNetworkPeerTask;

public:
    enum ErrorCode {
        NoError = 0,
        CheckFailed,
        NotEnoughFreeSpaceError
    };

    explicit QnCheckFreeSpacePeerTask(QObject* parent = nullptr);

    void setUpdateFiles(
        const QHash<nx::vms::api::SystemInformation, QString>& fileBySystemInformation);

protected:
    virtual void doStart() override;
    virtual void doCancel() override;

private:
    QHash<nx::vms::api::SystemInformation, QString> m_files;
    QHash<nx::vms::api::SystemInformation, qint64> m_requiredSpaceBySystemInformation;
    QHash<QnUuid, qint64> m_freeSpaceByServer;
    rest::Handle m_requestId = -1;
};
