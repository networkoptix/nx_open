#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>
#include <core/resource/media_server_resource.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <mutex/distributed_mutex.h>

class QNetworkAccessManager;
class QnUpdateUploader;

class QnDownloadUpdatesPeerTask;
class QnUploadUpdatesPeerTask;
class QnInstallUpdatesPeerTask;
class QnRestUpdatePeerTask;

class QnMediaServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct UpdateFileInformation {
        QnSoftwareVersion version;
        QString fileName;
        qint64 fileSize;
        QString baseFileName;
        QUrl url;
        QString md5;

        UpdateFileInformation() {}

        UpdateFileInformation(const QnSoftwareVersion &version, const QString &fileName) :
            version(version), fileName(fileName)
        {}

        UpdateFileInformation(const QnSoftwareVersion &version, const QUrl &url) :
            version(version), url(url)
        {}
    };
    typedef QSharedPointer<UpdateFileInformation> UpdateFileInformationPtr;

    struct PeerUpdateInformation {
        enum State {
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
        UpdateFileInformationPtr updateInformation;

        int progress;

        PeerUpdateInformation(const QnMediaServerResourcePtr &server = QnMediaServerResourcePtr());
    };

    enum State {
        Idle,
        CheckingForUpdates,
        DownloadingUpdate,
        InstallingToIncompatiblePeers,
        UploadingUpdate,
        InstallingUpdate,
    };

    enum CheckResult {
        UpdateFound,
        InternetProblem,
        NoNewerVersion,
        NoSuchBuild,
        UpdateImpossible
    };

    enum UpdateResult {
        UpdateSuccessful,
        Cancelled,
        LockFailed,
        DownloadingFailed,
        UploadingFailed,
        InstallationFailed,
        RestInstallationFailed
    };

    QnMediaServerUpdateTool(QObject *parent = 0);

    State state() const;

    bool isUpdating() const;

    CheckResult updateCheckResult() const;
    UpdateResult updateResult() const;
    QString resultString() const;

    void updateServers();

    QnSoftwareVersion targetVersion() const;

    void setDenyMajorUpdates(bool denyMajorUpdates);

    PeerUpdateInformation updateInformation(const QnId &peerId) const;

    QnMediaServerResourceList targets() const;
    void setTargets(const QSet<QnId> &targets);

    QnMediaServerResourceList actualTargets() const;

    QUrl generateUpdatePackageUrl() const;

signals:
    void stateChanged(int state);
    void progressChanged(int progress);
    void peerChanged(const QnId &peerId);

public slots:
    void checkForUpdates();
    void checkForUpdates(const QnSoftwareVersion &version);
    void checkForUpdates(const QString &path);
    bool cancelUpdate();

protected:
    void checkOnlineUpdates(const QnSoftwareVersion &version = QnSoftwareVersion());
    void checkLocalUpdates();
    void checkUpdateCoverage();

private slots:
    void at_updateReply_finished();
    void at_buildReply_finished();

    void at_mutexLocked();
    void at_mutexTimeout();

    void at_downloadTask_finished(int errorCode);
    void at_uploadTask_finished(int errorCode);
    void at_installTask_finished(int errorCode);
    void at_restUpdateTask_finished(int errorCode);
    void at_downloadTask_peerFinished(const QnId &peerId);
    void at_uploadTask_peerFinished(const QnId &peerId);
    void at_installTask_peerFinished(const QnId &peerId);
    void at_restUpdateTask_peerFinished(const QnId &peerId);

    void at_networkTask_peerProgressChanged(const QnId &peerId, int progress);

private:
    void setState(State state);
    void setCheckResult(CheckResult result);
    void setUpdateResult(UpdateResult result);
    void finishUpdate(UpdateResult result);
    void setPeerState(const QnId &peerId, PeerUpdateInformation::State state);
    void checkBuildOnline();

    void downloadUpdates();
    void uploadUpdatesToServers();
    void installUpdatesToServers();
    void installIncompatiblePeers();

    void lockMutex();
    void unlockMutex();

private:
    State m_state;
    CheckResult m_checkResult;
    UpdateResult m_updateResult;
    QString m_resultString;

    QDir m_localUpdateDir;
    QUrl m_onlineUpdateUrl;
    QString m_updateLocationPrefix;

    QnSoftwareVersion m_targetVersion;
    bool m_targetMustBeNewer;
    bool m_denyMajorUpdates;

    QString m_updateId;

    QHash<QnSystemInformation, UpdateFileInformationPtr> m_updateFiles;

    QNetworkAccessManager *m_networkAccessManager;
    ec2::QnDistributedMutex *m_distributedMutex;

    QHash<QnId, PeerUpdateInformation> m_updateInformationById;
    QMultiHash<QnSystemInformation, QnId> m_idBySystemInformation;

    QnMediaServerResourceList m_targets;

    QnDownloadUpdatesPeerTask *m_downloadUpdatesPeerTask;
    QnUploadUpdatesPeerTask *m_uploadUpdatesPeerTask;
    QnInstallUpdatesPeerTask *m_installUpdatesPeerTask;
    QnRestUpdatePeerTask *m_restUpdatePeerTask;

    QSet<QnId> m_incompatiblePeerIds;
    QSet<QnId> m_targetPeerIds;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
