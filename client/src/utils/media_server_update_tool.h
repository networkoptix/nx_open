#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>
#include <core/resource/media_server_resource.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

// TODO: add tracking to the newly added servers

class QNetworkAccessManager;

class QnMediaServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct UpdateInformation {
        QnSoftwareVersion version;
        QString fileName;
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
        UpdateFound,
        UpdateImpossible,
        CheckingFailed,
        CheckingForUpdates,
        DownloadingUpdate,
        UploadingUpdate,
        InstallingUpdate
    };

    QnMediaServerUpdateTool(QObject *parent = 0);

    State state() const;

    void updateServers();

    QHash<QnSystemInformation, UpdateInformation> availableUpdates() const;

signals:
    void stateChanged(int state);

public slots:
    void checkForUpdates();
    void checkForUpdates(const QnSoftwareVersion &version);
    void checkForUpdates(const QString &path);

protected:
    ec2::AbstractECConnectionPtr connection2() const;

    void checkOnlineUpdates(const QnSoftwareVersion &version = QnSoftwareVersion());
    void checkLocalUpdates();
    void checkUpdateCoverage();

private slots:
    void at_updateReply_finished();
    void at_buildReply_finished();
    void at_updateUploaded(const QString &updateId, const QnId &peerId);

private:
    void setState(State state);
    void checkBuildOnline();

private:
    State m_state;
    QDir m_localUpdateDir;
    QUrl m_onlineUpdateUrl;
    QString m_updateLocationPrefix;
    QnSoftwareVersion m_targetVersion;
    bool m_targetMustBeNewer;

    QHash<QString, QnMediaServerResourcePtr> m_pendingUploadServers;
    QHash<QString, QnMediaServerResourcePtr> m_pendingInstallServers;
    QHash<QnSystemInformation, UpdateInformation> m_updates;

    QNetworkAccessManager *m_networkAccessManager;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
