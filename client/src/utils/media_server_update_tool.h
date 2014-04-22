#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>
#include <core/resource/media_server_resource.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

// TODO: add tracking to the newly added servers

class QnMediaServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct UpdateInformation {
        QnSoftwareVersion version;
        QString fileName;

        UpdateInformation(const QnSoftwareVersion &version, const QString &fileName) :
            version(version), fileName(fileName)
        {}
    };

    enum UpdateMode {
        OnlineUpdate,
        SpecificBuildOnlineUpdate,
        LocalUpdate
    };

    enum State {
        Idle,
        CheckingForUpdates,
        UpdateFound,
        UpdateImpossible,
        DownloadingUpdate,
        UploadingUpdate,
        InstallingUpdate
    };

    QnMediaServerUpdateTool(QObject *parent = 0);

    State state() const;

    void updateServers();

    void setUpdateMode(UpdateMode mode);
    void setLocalUpdateDir(const QDir &dir);

    QHash<QnSystemInformation, UpdateInformation> availableUpdates() const;

signals:
    void stateChanged(int state);

public slots:
    void checkForUpdates();

protected:
    ec2::AbstractECConnectionPtr connection2() const;

    void checkOnlineUpdates();
    void checkLocalUpdates();

private slots:
    void at_updateUploaded(const QString &updateId, const QnId &peerId);

private:
    void setState(State state);

private:
    State m_state;
    UpdateMode m_updateMode;
    QDir m_localUpdateDir;
    QUrl m_onlineUpdateUrl;

    QHash<QString, QnMediaServerResourcePtr> m_pendingUploadServers;
    QHash<QString, QnMediaServerResourcePtr> m_pendingInstallServers;
    QHash<QnSystemInformation, UpdateInformation> m_updates;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
