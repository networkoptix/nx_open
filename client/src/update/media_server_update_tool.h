#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <mutex/distributed_mutex.h>

#include <update/updates_common.h>
#include <update/update_process.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

class QnCheckForUpdatesPeerTask;
class QnDownloadUpdatesPeerTask;
class QnUploadUpdatesPeerTask;
class QnInstallUpdatesPeerTask;
class QnRestUpdatePeerTask;

class QnMediaServerUpdateTool : public QObject {
    Q_OBJECT
public:
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

    void updateServers();

    QnSoftwareVersion targetVersion() const;

    QnPeerUpdateInformation updateInformation(const QUuid &peerId) const;

    QnMediaServerResourceList targets() const;
    void setTargets(const QSet<QUuid> &targets, bool client = false);

    QnMediaServerResourceList actualTargets() const;
    QSet<QUuid> actualTargetIds() const;

    /** Generate url for download update file, depending on actual targets list. */
    QUrl generateUpdatePackageUrl(const QnSoftwareVersion &targetVersion) const;

    void reset();

    bool isClientRequiresInstaller() const;
signals:
    void stateChanged(int state);
    void progressChanged(int progress);
    void peerChanged(const QUuid &peerId);
    void targetsChanged(const QSet<QUuid> &targets);

    void checkForUpdatesFinished(QnCheckForUpdateResult result);
    void updateFinished(QnUpdateResult result);

public slots:
    void checkForUpdates(const QnSoftwareVersion &version = QnSoftwareVersion(), bool denyMajorUpdates = false);
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
    void at_restUpdateTask_peerUpdateFinished(const QUuid &incompatibleId, const QUuid &id);
    void at_clientUpdateInstalled();

    void at_taskProgressChanged(int progress);
    void at_networkTask_peerProgressChanged(const QUuid &peerId, int progress);

    void at_resourcePool_statusChanged(const QnResourcePtr &resource);

private:
    void setState(State state);
    void finishUpdate(QnUpdateResult result);
    void setPeerState(const QUuid &peerId, QnPeerUpdateInformation::State state);
    void removeTemporaryDir();

    void downloadUpdates();
    void uploadUpdatesToServers();
    void installClientUpdate();
    void installUpdatesToServers();
    void installIncompatiblePeers();

    void prepareToUpload();
    void lockMutex();
    void unlockMutex();

private:
    QThread *m_tasksThread;
    State m_state;
    
    bool m_targetMustBeNewer;

    QnUpdateProcess m_updateProcess;

    ec2::QnDistributedMutex *m_distributedMutex;

    QnMediaServerResourceList m_targets;

    QnCheckForUpdatesPeerTask *m_checkForUpdatesPeerTask;
    QnDownloadUpdatesPeerTask *m_downloadUpdatesPeerTask;
    QnUploadUpdatesPeerTask *m_uploadUpdatesPeerTask;
    QnInstallUpdatesPeerTask *m_installUpdatesPeerTask;
    QnRestUpdatePeerTask *m_restUpdatePeerTask;

    QSet<QUuid> m_incompatiblePeerIds;
    QSet<QUuid> m_targetPeerIds;

    
    bool m_disableClientUpdates;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
