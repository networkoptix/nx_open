#pragma once

#include <map>

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <update/task/network_peer_task.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data_fwd.h>

class QTimer;
struct QnUploadUpdateReply;

class QnInstallUpdatesPeerTask: public QnNetworkPeerTask
{
    Q_OBJECT

public:
    // This map is used to get the peers sorted by distance in descending order.
    using PeersToQueryMap = std::multimap<int, QnUuid, std::greater<int>>;

    enum ErrorCode
    {
        NoError = 0,
        InstallationFailed
    };

    explicit QnInstallUpdatesPeerTask(QObject* parent = nullptr);

    void setUpdateId(const QString& updateId);
    void setVersion(const nx::utils::SoftwareVersion& version);

signals:
    void protocolProblemDetected();

protected:
    virtual void doStart() override;

private slots:
    void at_resourceStatusChanged(const QnResourcePtr& resource);
    void at_serverVersionChanged(const QnResourcePtr& resource);
    void at_checkTimer_timeout();
    void at_pingTimer_timeout();
    void at_gotModuleInformation(
        int status, const QList<nx::vms::api::ModuleInformation>& modules, int handle);
    void at_installUpdateResponse(
        int status, const QnUploadUpdateReply& reply, int handle);

private:
    void queryNextGroup();
    void startWaiting();
    void removeWaitingPeer(const QnUuid& id);
    void finish(int errorCode, const QSet<QnUuid>& failedPeers = QSet<QnUuid>());

private:
    QString m_updateId;
    QnMediaServerResourcePtr m_ecServer;
    QnMediaServerConnectionPtr m_ecConnection;
    nx::utils::SoftwareVersion m_version;
    QSet<QnUuid> m_stoppingPeers;
    QSet<QnUuid> m_restartingPeers;
    QSet<QnUuid> m_pendingPeers;
    QTimer* m_checkTimer = nullptr;
    QTimer* m_pingTimer = nullptr;
    bool m_protoProblemDetected = false;
    QHash<int, QnMediaServerResourcePtr> m_serverByRequest;
    PeersToQueryMap m_peersToQuery;
};
