#ifndef QN_UPDATE_PROCESS_H
#define QN_UPDATE_PROCESS_H

#include <core/resource/resource_fwd.h>

#include <update/updates_common.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <utils/common/long_runnable.h>

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
class QnCheckForUpdatesPeerTask;

namespace ec2 {
    class QnDistributedMutex;
}

class QnUpdateProcess: public QnLongRunnable {
    Q_OBJECT

    typedef QnLongRunnable base_type;
public:
    QnUpdateProcess(const QnUpdateTarget &target);


    virtual void pleaseStop() override;
protected:
    virtual void run() override;

signals:
    void stageChanged(QnFullUpdateStage stage);
    void peerStageChanged(const QUuid &peerId, QnPeerUpdateStage stage);

    void progressChanged(int progress);
    void peerStageProgressChanged(const QUuid &peerId, QnPeerUpdateStage stage, int progress);

    void targetsChanged(const QSet<QUuid> &targets);

    void updateFinished(const QnUpdateResult &result);
private:
    void setStage(QnFullUpdateStage stage);

    void setPeerState(const QUuid &peerId, QnPeerUpdateInformation::State state);
    void setAllPeersState(QnPeerUpdateInformation::State state);
    void setCompatiblePeersState(QnPeerUpdateInformation::State state);
    void setIncompatiblePeersState(QnPeerUpdateInformation::State state);

    void downloadUpdates();
    void installClientUpdate();
    void installIncompatiblePeers();
    void uploadUpdatesToServers();
    void installUpdatesToServers();

    void finishUpdate(QnUpdateResult::Value value);

    void prepareToUpload();
    void lockMutex();
    void unlockMutex();

    void removeTemporaryDir();
private:
    void at_checkForUpdatesTaskFinished(QnCheckForUpdatesPeerTask* task, const QnCheckForUpdateResult &result);
    void at_downloadTaskFinished(QnDownloadUpdatesPeerTask* task, int errorCode);
    void at_restUpdateTask_finished(int errorCode);
    void at_uploadTask_finished(int errorCode);
    void at_installTask_finished(int errorCode);

    void at_restUpdateTask_peerUpdateFinished(const QUuid &incompatibleId, const QUuid &id);
    void at_clientUpdateInstalled();

    void at_mutexLocked();
    void at_mutexTimeout();
private:
    const QUuid m_id;
    QnUpdateTarget m_target;
    QPointer<QnNetworkPeerTask> m_currentTask;
    QnFullUpdateStage m_stage;
    ec2::QnDistributedMutex *m_distributedMutex;
    QString m_localTemporaryDir;
    QSet<QUuid> m_incompatiblePeerIds;
    QSet<QUuid> m_targetPeerIds;
    bool m_clientRequiresInstaller;
    QHash<QnSystemInformation, QnUpdateFileInformationPtr> m_updateFiles;
    QnUpdateFileInformationPtr m_clientUpdateFile;
    QHash<QUuid, QnPeerUpdateInformation> m_updateInformationById;
    QMultiHash<QnSystemInformation, QUuid> m_idBySystemInformation;
    QnUpdateResult::Value m_updateResult;
};


#endif //QN_UPDATE_PROCESS_H