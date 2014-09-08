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
    QnMediaServerUpdateTool(QObject *parent = 0);
    ~QnMediaServerUpdateTool();

    QnFullUpdateStage stage() const;

    bool isUpdating() const;
    bool idle() const;

    QnMediaServerResourceList targets() const;
    void setTargets(const QSet<QUuid> &targets, bool client = false);

    QnMediaServerResourceList actualTargets() const;
    QSet<QUuid> actualTargetIds() const;

    /** Generate url for download update file, depending on actual targets list. */
    QUrl generateUpdatePackageUrl(const QnSoftwareVersion &targetVersion) const;
signals:
    void stageChanged(QnFullUpdateStage stage);
    void progressChanged(int progress);
    void peerStageChanged(const QUuid &peerId, QnPeerUpdateStage stage);
    void peerProgressChanged(const QUuid &peerId, int progress);

    void targetsChanged(const QSet<QUuid> &targets);

    void checkForUpdatesFinished(const QnCheckForUpdateResult &result);
    void updateFinished(QnUpdateResult result);

public slots:
    void checkForUpdates(const QnSoftwareVersion &version = QnSoftwareVersion(), bool denyMajorUpdates = false);
    void checkForUpdates(const QString &fileName);

    void startUpdate(const QnSoftwareVersion &version = QnSoftwareVersion(), bool denyMajorUpdates = false);
    void startUpdate(const QString &fileName);

    bool cancelUpdate();

private:
    void startUpdate(const QnUpdateTarget &target);
    void checkForUpdates(const QnUpdateTarget &target);

    void setStage(QnFullUpdateStage stage);
    void finishUpdate(QnUpdateResult result);
    
private:
    QnFullUpdateStage m_stage;
    
    QnUpdateProcess* m_updateProcess;

    QnMediaServerResourceList m_targets;
    
    bool m_disableClientUpdates;
};

#endif // QN_MEDIA_SERVER_UPDATE_TOOL_H
