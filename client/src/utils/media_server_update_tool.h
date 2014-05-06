#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>
#include <core/resource/media_server_resource.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <mutex/distributed_mutex.h>

// TODO: add tracking to the newly added servers

class QNetworkAccessManager;

class QnMediaServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct UpdateFileInformation {
        QnSoftwareVersion version;
        QString fileName;
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

    PeerUpdateInformation updateInformation(const QnId &peerId) const;

signals:
    void stateChanged(int state);
    void progressChanged(int progress);
    void serverProgressChanged(const QnMediaServerResourcePtr &server, int progress);
    void peerChanged(const QnId &peerId);

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
    void at_updateUploaded(const QString &updateId, const QnId &peerId);

    void at_downloadReply_downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadNextUpdate();

    void at_mutexLocked(const QByteArray &);
    void at_mutexTimeout(const QByteArray &);

    void at_resourceChanged(const QnResourcePtr &resource);
    void at_installationTimeout();

private:
    void setState(State state);
    void setCheckResult(CheckResult result);
    void setUpdateResult(UpdateResult result);
    void setPeerState(const QnId &peerId, PeerUpdateInformation::State state);
    void checkBuildOnline();

    void uploadUpdatesToServers();
    void installUpdatesToServers();

private:
    State m_state;
    CheckResult m_checkResult;
    UpdateResult m_updateResult;

    QDir m_localUpdateDir;
    QUrl m_onlineUpdateUrl;
    QString m_updateLocationPrefix;

    QnSoftwareVersion m_targetVersion;
    bool m_targetMustBeNewer;

    QMultiHash<QnSystemInformation, QnId> m_idBySystemInformation;

    QSet<QnId> m_pendingUploads;
    QSet<QnId> m_pendingInstallations;

    QHash<QnSystemInformation, UpdateFileInformationPtr> m_updateFiles;
    QSet<QnSystemInformation> m_downloadingUpdates;

    QNetworkAccessManager *m_networkAccessManager;
    ec2::QnDistributedMutexPtr m_distributedMutex;

    QHash<QnId, PeerUpdateInformation> m_updateInformationById;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
