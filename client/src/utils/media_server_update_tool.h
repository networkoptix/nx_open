#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>
#include <core/resource/media_server_resource.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <utils/updates_common.h>
#include <mutex/distributed_mutex.h>

class QnUpdateUploader;

class QnCheckForUpdatesPeerTask;
class QnDownloadUpdatesPeerTask;
class QnUploadUpdatesPeerTask;
class QnInstallUpdatesPeerTask;
class QnRestUpdatePeerTask;

class QnMediaServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct PeerUpdateInformation {
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

        PeerUpdateInformation(const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr());
    };

    enum State {
        Idle,
        CheckingForUpdates,
        DownloadingUpdate,
        InstallingToIncompatiblePeers,
        UploadingUpdate,
        InstallingClientUpdate,
        InstallingUpdate,
    };

    QnMediaServerUpdateTool(QObject *parent = 0);
    ~QnMediaServerUpdateTool();

    State state() const;

    bool isUpdating() const;
    bool idle() const;

    QnCheckForUpdateResult updateCheckResult() const;
    QnUpdateResult updateResult() const;

    void updateServers();

    QnSoftwareVersion targetVersion() const;

    void setDenyMajorUpdates(bool denyMajorUpdates);

    PeerUpdateInformation updateInformation(const QUuid &peerId) const;

    QnMediaServerResourceList targets() const;
    void setTargets(const QSet<QUuid> &targets, bool client = false);

    QnMediaServerResourceList actualTargets() const;

    QUrl generateUpdatePackageUrl() const;

    void reset();

    bool isClientRequiresInstaller() const;

    bool canStartUpdate() const;

signals:
    void stateChanged(int state);
    void progressChanged(int progress);
    void peerChanged(const QUuid &peerId);

    void checkForUpdatesFinished(QnCheckForUpdateResult result);
    void updateFinished(QnUpdateResult result);

public slots:
    void checkForUpdates(const QnSoftwareVersion &version = QnSoftwareVersion());
    void checkForUpdates(const QString &fileName);
    bool cancelUpdate();

private slots:
    void at_mutexLocked();
    void at_mutexTimeout();

    void at_checkForUpdatesTask_finished(int errorCode);
    void at_downloadTask_finished(int errorCode);
    void at_uploadTask_finished(int errorCode);
    void at_installTask_finished(int errorCode);
    void at_restUpdateTask_finished(int errorCode);
    void at_downloadTask_peerFinished(const QUuid &peerId);
    void at_uploadTask_peerFinished(const QUuid &peerId);
    void at_installTask_peerFinished(const QUuid &peerId);
    void at_restUpdateTask_peerFinished(const QUuid &peerId);
    void at_clientUpdateInstalled();

    void at_networkTask_peerProgressChanged(const QUuid &peerId, int progress);

private:
    void setState(State state);
    void setCheckResult(QnCheckForUpdateResult result);
    void setUpdateResult(QnUpdateResult result);
    void finishUpdate(QnUpdateResult result);
    void setPeerState(const QUuid &peerId, PeerUpdateInformation::State state);
    void removeTemporaryDir();

    void downloadUpdates();
    void uploadUpdatesToServers();
    void installClientUpdate();
    void installUpdatesToServers();
    void installIncompatiblePeers();

    void lockMutex();
    void unlockMutex();

private:
    QThread *m_tasksThread;
    State m_state;
    QnCheckForUpdateResult m_checkResult;
    QnUpdateResult m_updateResult;

    QString m_localTemporaryDir;

    QnSoftwareVersion m_targetVersion;
    bool m_targetMustBeNewer;
    bool m_denyMajorUpdates;

    QString m_updateId;

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> m_updateFiles;
    QnUpdateFileInformationPtr m_clientUpdateFile;

    ec2::QnDistributedMutex *m_distributedMutex;

    QHash<QUuid, PeerUpdateInformation> m_updateInformationById;
    QMultiHash<QnSystemInformation, QUuid> m_idBySystemInformation;

    QnMediaServerResourceList m_targets;

    QnCheckForUpdatesPeerTask *m_checkForUpdatesPeerTask;
    QnDownloadUpdatesPeerTask *m_downloadUpdatesPeerTask;
    QnUploadUpdatesPeerTask *m_uploadUpdatesPeerTask;
    QnInstallUpdatesPeerTask *m_installUpdatesPeerTask;
    QnRestUpdatePeerTask *m_restUpdatePeerTask;

    QSet<QUuid> m_incompatiblePeerIds;
    QSet<QUuid> m_targetPeerIds;

    bool m_clientRequiresInstaller;
    bool m_disableClientUpdates;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
