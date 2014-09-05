#ifndef CHECK_UPDATE_PEER_TASK_H
#define CHECK_UPDATE_PEER_TASK_H

#include <update/task/network_peer_task.h>
#include <update/updates_common.h>

#include <utils/common/system_information.h>

class QNetworkAccessManager;

class QnCheckForUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT
public:
    explicit QnCheckForUpdatesPeerTask(QObject *parent = 0);

    void setUpdatesUrl(const QUrl &url);

    void setTargetVersion(const QnSoftwareVersion &version);
    QnSoftwareVersion targetVersion() const;

    void setUpdateFileName(const QString &fileName);
    QString updateFileName() const;

    void setDisableClientUpdates(bool f);
    bool isDisableClientUpdates() const;

    bool isClientRequiresInstaller() const;

    void setDenyMajorUpdates(bool denyMajorUpdates);

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> updateFiles() const;
    QnUpdateFileInformationPtr clientUpdateFile() const;
    QString temporaryDir() const;

protected:
    virtual void doStart() override;

private:
    bool needUpdate(const QnSoftwareVersion &version, const QnSoftwareVersion &updateVersion) const;

    void checkUpdateCoverage();

    void checkBuildOnline();
    void checkOnlineUpdates(const QnSoftwareVersion &version = QnSoftwareVersion());
    void checkLocalUpdates();

    void cleanUp();

    void finishTask(QnCheckForUpdateResult result);
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
    bool m_disableClientUpdates;

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> m_updateFiles;
    QnUpdateFileInformationPtr m_clientUpdateFile;
    bool m_clientRequiresInstaller;
};

#endif // CHECK_UPDATE_PEER_TASK_H
