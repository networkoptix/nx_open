#pragma once

#include <update/task/network_peer_task.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/system_information.h>
#include <utils/common/software_version.h>
#include <core/resource/fake_media_server.h>

struct QnUploadUpdateReply;

class QnRestUpdatePeerTask: public QnNetworkPeerTask
{
    Q_OBJECT

public:
    enum ErrorCode
    {
        NoError = 0,
        ParametersError,
        FileError,
        UploadError,
        InstallationError
    };

    explicit QnRestUpdatePeerTask(QObject* parent = nullptr);

    void setUpdateId(const QString& updateId);
    void setVersion(const QnSoftwareVersion& version);

signals:
    void peerUpdateFinished(const QnUuid& incompatibleId, const QnUuid& id);

protected:
    virtual void doCancel() override;
    virtual void doStart() override;

private:
    void installNextUpdate();
    void finishPeer(const QnUuid& id);

private slots:
    void at_updateInstalled(int status, const QnUploadUpdateReply& reply, int handle);
    void at_resourceChanged(const QnResourcePtr& resource);
    void at_timer_timeout();

private:
    QString m_updateId;
    QnSoftwareVersion m_version;
    QHash<int, QnMediaServerResourcePtr> m_serverByRequest;
    QHash<QnUuid, QnFakeMediaServerResourcePtr> m_serverByRealId;
    QTimer* m_timer;
};
