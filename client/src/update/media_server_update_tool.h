#ifndef QN_MEDIA_SERVER_UPDATE_TOOL_H
#define QN_MEDIA_SERVER_UPDATE_TOOL_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <update/updates_common.h>
#include <update/update_process.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

class QnCheckForUpdatesPeerTask;
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
    void at_checkForUpdatesTask_finished(int errorCode);
        
    void at_taskProgressChanged(int progress);
    void at_networkTask_peerProgressChanged(const QUuid &peerId, int progress);
private:
    void setState(State state);
    void finishUpdate(QnUpdateResult result);
    
    void removeTemporaryDir();

private:
    QThread *m_tasksThread;
    State m_state;
    
    bool m_targetMustBeNewer;

    QnUpdateProcess* m_updateProcess;

    QnMediaServerResourceList m_targets;

    QnCheckForUpdatesPeerTask *m_checkForUpdatesPeerTask;

    
    bool m_disableClientUpdates;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
