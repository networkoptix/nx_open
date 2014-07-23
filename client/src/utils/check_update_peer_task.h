#ifndef CHECK_UPDATE_PEER_TASK_H
#define CHECK_UPDATE_PEER_TASK_H

#include <utils/network_peer_task.h>
#include <utils/common/system_information.h>
#include <utils/updates_common.h>

class QNetworkAccessManager;

class QnCheckForUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    enum CheckResult {
        UpdateFound = 0,
        InternetProblem,
        NoNewerVersion,
        NoSuchBuild,
        UpdateImpossible,
        BadUpdateFile
    };

    explicit QnCheckForUpdatesPeerTask(QObject *parent = 0);

    void setUpdatesUrl(const QUrl &url);

    void setTargetVersion(const QnSoftwareVersion &version);
    QnSoftwareVersion targetVersion() const;

    void setUpdateFileName(const QString &fileName);
    QString updateFileName() const;

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> updateFiles() const;
    QString temporaryDir() const;

    bool needUpdate(const QnSoftwareVersion &version, const QnSoftwareVersion &updateVersion) const;

protected:
    virtual void doStart() override;

private:
    void checkUpdateCoverage();

    void checkBuildOnline();
    void checkOnlineUpdates(const QnSoftwareVersion &version = QnSoftwareVersion());
    void checkLocalUpdates();

    void cleanUp();

private slots:
    void at_updateReply_finished();
    void at_buildReply_finished();

private:
    QNetworkAccessManager *m_networkAccessManager;
    QUrl m_updatesUrl;

    QnSoftwareVersion m_targetVersion;
    QString m_updateFileName;
    QString m_temporaryUpdateDir;

    QString m_updateLocationPrefix;
    bool m_targetMustBeNewer;
    bool m_denyMajorUpdates;

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> m_updateFiles;
};

#endif // CHECK_UPDATE_PEER_TASK_H
