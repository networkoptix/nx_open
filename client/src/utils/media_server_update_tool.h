#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>
#include <core/resource/media_server_resource.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <mutex/distributed_mutex.h>

class QNetworkAccessManager;
class QnUpdateUploader;

class QnMediaServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct UpdateFileInformation {
        QnSoftwareVersion version;
        QString fileName;
        qint64 fileSize;
        QString baseFileName;
        QUrl url;

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
        InstallationFailed
    };

    QnMediaServerUpdateTool(QObject *parent = 0);

    State state() const;

    bool isUpdating() const;

    CheckResult updateCheckResult() const;
    UpdateResult updateResult() const;

    void updateServers();

    QnSoftwareVersion targetVersion() const;

    PeerUpdateInformation updateInformation(const QUuid &peerId) const;

signals:
    void stateChanged(int state);
    void progressChanged(int progress);
    void peerChanged(const QUuid &peerId);

public slots:
    void checkForUpdates();
    void checkForUpdates(const QnSoftwareVersion &version);
    void checkForUpdates(const QString &path);
    bool cancelUpdate();

protected:
    ec2::AbstractECConnectionPtr connection2() const;

    void checkOnlineUpdates(const QnSoftwareVersion &version = QnSoftwareVersion());
    void checkLocalUpdates();
    void checkUpdateCoverage();

private slots:
    void at_updateReply_finished();
    void at_buildReply_finished();
    void at_downloadReply_finished();

    void uploadNextUpdate();
    void at_uploader_finished();
    void at_uploader_failed();
    void at_uploader_progressChanged(int progress);
    void at_uploader_peerProgressChanged(const QUuid &peerId, int progress);


    void at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadNextUpdate();

    void at_mutexLocked();
    void at_mutexTimeout();

    void at_resourceChanged(const QnResourcePtr &resource);
    void at_installationTimeout();

private:
    void setState(State state);
    void setCheckResult(CheckResult result);
    void setUpdateResult(UpdateResult result);
    void finishUpdate(UpdateResult result);
    void setPeerState(const QUuid &peerId, PeerUpdateInformation::State state);
    void checkBuildOnline();

    void uploadUpdatesToServers();
    void installUpdatesToServers();

    void unlockMutex();

private:
    State m_state;
    CheckResult m_checkResult;
    UpdateResult m_updateResult;

    QDir m_localUpdateDir;
    QUrl m_onlineUpdateUrl;
    QString m_updateLocationPrefix;

    QScopedPointer<QFile> m_downloadFile;

    QnSoftwareVersion m_targetVersion;
    bool m_targetMustBeNewer;

    QString m_updateId;

    QMultiHash<QnSystemInformation, QUuid> m_idBySystemInformation;

    QList<QnSystemInformation> m_pendingUploads;
    QSet<QUuid> m_pendingUploadPeers;
    QSet<QUuid> m_pendingInstallations;

    QnUpdateUploader *m_uploader;

    QHash<QnSystemInformation, UpdateFileInformationPtr> m_updateFiles;
    QSet<QnSystemInformation> m_downloadingUpdates;

    QNetworkAccessManager *m_networkAccessManager;
    ec2::QnDistributedMutex* m_distributedMutex;

    QSet<QUuid> m_restartingServers;

    QHash<QUuid, PeerUpdateInformation> m_updateInformationById;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
