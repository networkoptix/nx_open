#ifndef QN_UPDATE_PROCESS_H
#define QN_UPDATE_PROCESS_H

#include <core/resource/resource_fwd.h>

#include <update/updates_common.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <utils/common/long_runnable.h>

struct QnPeerUpdateInformation {
//     enum State {
//         UpdateUnknown,
//         UpdateNotFound,
//         UpdateFound,
//         PendingDownloading,
//         UpdateDownloading,
//         PendingUpload,
//         UpdateUploading,
//         PendingInstallation,
//         UpdateInstalling,
//         UpdateFinished,
//         UpdateFailed,
//         UpdateCanceled
//     };

    QnMediaServerResourcePtr server;
    QnPeerUpdateStage stage;
    QnSoftwareVersion sourceVersion;
    QnUpdateFileInformationPtr updateInformation;

    int progress;

    QnPeerUpdateInformation(const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr());
};

class QnNetworkPeerTask;
class QnDownloadUpdatesPeerTask;
class QnCheckForUpdatesPeerTask;
class QnPeerRuntimeInfo;

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
    void peerStageChanged(const QnUuid &peerId, QnPeerUpdateStage stage);

    void progressChanged(int progress);
    void peerStageProgressChanged(const QnUuid &peerId, QnPeerUpdateStage stage, int progress);

    void targetsChanged(const QSet<QnUuid> &targets);

    void updateFinished(const QnUpdateResult &result);
private:
    void setStage(QnFullUpdateStage stage);

    void setPeerStage(const QnUuid &peerId, QnPeerUpdateStage stage);
    void setAllPeersStage(QnPeerUpdateStage stage);
    void setCompatiblePeersStage(QnPeerUpdateStage stage);
    void setIncompatiblePeersStage(QnPeerUpdateStage stage);

    void downloadUpdates();
    void installClientUpdate();
    void installIncompatiblePeers();
    void uploadUpdatesToServers();
    void installUpdatesToServers();

    void finishUpdate(QnUpdateResult::Value value);

    void prepareToUpload();
    bool setUpdateFlag();
    void clearUpdateFlag();

private:
    void at_checkForUpdatesTaskFinished(QnCheckForUpdatesPeerTask* task, const QnCheckForUpdateResult &result);
    void at_downloadTaskFinished(QnDownloadUpdatesPeerTask* task, int errorCode);
    void at_restUpdateTask_finished(int errorCode);
    void at_uploadTask_finished(int errorCode, const QSet<QnUuid> &failedPeers);
    void at_installTask_finished(int errorCode);

    void at_restUpdateTask_peerUpdateFinished(const QnUuid &incompatibleId, const QnUuid &id);
    void at_clientUpdateInstalled();

    void at_runtimeInfoChanged(const QnPeerRuntimeInfo &data);

private:
    QString m_id;
    QnUpdateTarget m_target;
    QPointer<QnNetworkPeerTask> m_currentTask;
    QnFullUpdateStage m_stage;
    QSet<QnUuid> m_incompatiblePeerIds;
    QSet<QnUuid> m_targetPeerIds;
    bool m_clientRequiresInstaller;
    QHash<QnSystemInformation, QnUpdateFileInformationPtr> m_updateFiles;
    QnUpdateFileInformationPtr m_clientUpdateFile;
    QHash<QnUuid, QnPeerUpdateInformation> m_updateInformationById;
    QMultiHash<QnSystemInformation, QnUuid> m_idBySystemInformation;
    QnUpdateResult::Value m_updateResult;
    QSet<QnUuid> m_failedPeerIds;
};


#endif //QN_UPDATE_PROCESS_H
