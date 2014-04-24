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
    struct UpdateInformation {
        QnSoftwareVersion version;
        QString fileName;
        QString baseFileName;
        QUrl url;

        UpdateInformation() {}

        UpdateInformation(const QnSoftwareVersion &version, const QString &fileName) :
            version(version), fileName(fileName)
        {}

        UpdateInformation(const QnSoftwareVersion &version, const QUrl &url) :
            version(version), url(url)
        {}
    };

    enum State {
        Idle,
        CheckingForUpdates,
        DownloadingUpdate,
        UploadingUpdate,
        InstallingUpdate
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

    CheckResult updateCheckResult() const;

    void updateServers();

    QHash<QnSystemInformation, UpdateInformation> availableUpdates() const;

    QnSoftwareVersion targetVersion() const;

signals:
    void stateChanged(int state);
    void progressChanged(int progress);

public slots:
    void checkForUpdates();
    void checkForUpdates(const QnSoftwareVersion &version);
    void checkForUpdates(const QString &path);
    void cancelUpdate();

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

private:
    void setState(State state);
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

    QHash<QnId, QnMediaServerResourcePtr> m_pendingUploadServers;
    QMultiHash<QnSystemInformation, QnId> m_serverIdBySystemInformation;

    QHash<QnId, QnMediaServerResourcePtr> m_pendingInstallServers;

    QHash<QnSystemInformation, UpdateInformation> m_updates;
    QHash<QnSystemInformation, UpdateInformation> m_downloadingUpdates;

    QNetworkAccessManager *m_networkAccessManager;
    ec2::QnDistributedMutexPtr m_distributedMutex;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
