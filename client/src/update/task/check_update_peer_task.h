#ifndef CHECK_UPDATE_PEER_TASK_H
#define CHECK_UPDATE_PEER_TASK_H

#include <update/task/network_peer_task.h>
#include <update/updates_common.h>

#include <utils/common/system_information.h>
#include <utils/network/http/asynchttpclient.h>

class QnAsyncHttpClientReply;

class QnCheckForUpdatesPeerTask : public QnNetworkPeerTask {
    Q_OBJECT

    typedef QnNetworkPeerTask base_type;
public:
    explicit QnCheckForUpdatesPeerTask(const QnUpdateTarget &target, QObject *parent = 0);

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> updateFiles() const;
    QnUpdateFileInformationPtr clientUpdateFile() const;

    void start();

    QnUpdateTarget target() const;

signals:
    void checkFinished(const QnCheckForUpdateResult &result);

protected:
    virtual void doStart() override;

private:
    bool needUpdate(const QnSoftwareVersion &version, const QnSoftwareVersion &updateVersion) const;

    void checkUpdateCoverage();

    void checkBuildOnline();
    void checkOnlineUpdates();
    void checkLocalUpdates();

    void cleanUp();

    void finishTask(QnCheckForUpdateResult::Value result);

    void loadServersFromSettings();
    bool tryNextServer();

private slots:
    void at_updateReply_finished(QnAsyncHttpClientReply *reply);
    void at_buildReply_finished(QnAsyncHttpClientReply *reply);
    void at_zipExtractor_finished(int error);

private:
    struct UpdateServerInfo {
        QUrl url;
    };

    QUrl m_mainUpdateUrl;
    QUrl m_currentUpdateUrl;
    QList<UpdateServerInfo> m_updateServers;

    QnUpdateTarget m_target;

    QString m_updateLocationPrefix;
    bool m_targetMustBeNewer;
    bool m_checkLatestVersion;

    QHash<QnSystemInformation, QnUpdateFileInformationPtr> m_updateFiles;
    QnUpdateFileInformationPtr m_clientUpdateFile;
    bool m_clientRequiresInstaller;

    QUrl m_releaseNotesUrl;

    QSet<QnAsyncHttpClientReply*> m_runningRequests;
};

#endif // CHECK_UPDATE_PEER_TASK_H
