#ifndef QN_UPDATE_PROCESS_H
#define QN_UPDATE_PROCESS_H

#include <core/resource/resource_fwd.h>

#include <update/updates_common.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

struct QnPeerUpdateInformation {
    enum State {
        UpdateUnknown,
        UpdateNotFound,
        UpdateFound,
        PendingDownloading,
        UpdateDownloading,
        PendingUpload,
        UpdateUploading,
        PendingInstallation,
        UpdateInstalling,
        UpdateFinished,
        UpdateFailed,
        UpdateCanceled
    };

    QnMediaServerResourcePtr server;
    State state;
    QnSoftwareVersion sourceVersion;
    QnUpdateFileInformationPtr updateInformation;

    int progress;

    QnPeerUpdateInformation(const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr());
};

class QnNetworkPeerTask;
class QnDownloadUpdatesPeerTask;
namespace ec2 {
    class QnDistributedMutex;
}

class QnUpdateProcess: public QObject {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnUpdateProcess(const QnMediaServerResourceList &targets, QObject *parent = NULL);

    void downloadUpdates();
    bool cancel();


    const QUuid id;

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> updateFiles;
    QString localTemporaryDir;
    QnSoftwareVersion targetVersion;
    bool clientRequiresInstaller;
    bool disableClientUpdates;
    QnUpdateFileInformationPtr clientUpdateFile;
    QHash<QUuid, QnPeerUpdateInformation> updateInformationById;
    QMultiHash<QnSystemInformation, QUuid> idBySystemInformation;

    QSet<QUuid> m_incompatiblePeerIds;
    QSet<QUuid> m_targetPeerIds;

signals:
    void stageChanged(QnFullUpdateStage stage);

    void peerChanged(const QUuid &peerId);
    void taskProgressChanged(int progress);
    void networkTask_peerProgressChanged(const QUuid &peerId, int progress);
    void targetsChanged(const QSet<QUuid> &targets);

    void updateFinished(QnUpdateResult result);
private:
    void setStage(QnFullUpdateStage stage);

    void setPeerState(const QUuid &peerId, QnPeerUpdateInformation::State state);
    void setPeersState(QnPeerUpdateInformation::State state);
    void setAllPeersState(QnPeerUpdateInformation::State state);
    void setIncompatiblePeersState(QnPeerUpdateInformation::State state);

    void installClientUpdate();
    void installIncompatiblePeers();
    void uploadUpdatesToServers();
    void installUpdatesToServers();

    void finishUpdate(QnUpdateResult result);

    void prepareToUpload();
    void lockMutex();
    void unlockMutex();
private:
    void at_downloadTaskFinished(QnDownloadUpdatesPeerTask* task, int errorCode);
    void at_restUpdateTask_finished(int errorCode);
    void at_uploadTask_finished(int errorCode);
    void at_installTask_finished(int errorCode);

    void at_restUpdateTask_peerUpdateFinished(const QUuid &incompatibleId, const QUuid &id);
    void at_clientUpdateInstalled();

    void at_mutexLocked();
    void at_mutexTimeout();
private:
    QPointer<QnNetworkPeerTask> m_currentTask;
    QnFullUpdateStage m_stage;
    ec2::QnDistributedMutex *m_distributedMutex;
};


#endif //QN_UPDATE_PROCESS_H