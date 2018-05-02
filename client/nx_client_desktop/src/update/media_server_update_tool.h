#pragma once

#include <functional>

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>

#include <update/updates_common.h>
#include <update/update_process.h>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <utils/common/connective.h>

class QnCheckForUpdatesPeerTask;
class QnUploadUpdatesPeerTask;
class QnInstallUpdatesPeerTask;
class QnRestUpdatePeerTask;
struct QnLowFreeSpaceWarning;

class QnMediaServerUpdateTool: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;
public:
    QnMediaServerUpdateTool(QObject* parent = nullptr);
    ~QnMediaServerUpdateTool();

    QnFullUpdateStage stage() const;

    bool isUpdating() const;
    bool idle() const;
    bool isCheckingUpdates() const;

    QnMediaServerResourceList targets() const;
    void setTargets(const QSet<QnUuid> &targets, bool client = false);

    QnMediaServerResourceList actualTargets() const;
    QSet<QnUuid> actualTargetIds() const;

    /** Generate url for download update file, depending on actual targets list. */
    QUrl generateUpdatePackageUrl(const QnSoftwareVersion &targetVersion,
        const QString& targetChangeset) const;

    void checkForUpdates(const QnSoftwareVersion &version = QnSoftwareVersion(), std::function<void(const QnCheckForUpdateResult &result)> func = NULL);
    void checkForUpdates(const QString &fileName, std::function<void(const QnCheckForUpdateResult &result)> func = NULL);

    void startUpdate(const QnSoftwareVersion &version = QnSoftwareVersion());
    void startUpdate(const QString &fileName);
    void startOnlineClientUpdate(const QnSoftwareVersion &version);

    bool canCancelUpdate() const;
    bool cancelUpdate();
    bool cancelUpdatesCheck();

signals:
    void stageChanged(QnFullUpdateStage stage);
    void stageProgressChanged(QnFullUpdateStage stage, int progress);
    void peerStageChanged(const QnUuid &peerId, QnPeerUpdateStage stage);
    void peerStageProgressChanged(const QnUuid &peerId, QnPeerUpdateStage stage, int progress);

    void targetsChanged(const QSet<QnUuid> &targets);

    void checkForUpdatesFinished(const QnCheckForUpdateResult &result);
    void updateFinished(QnUpdateResult result);

    void updatesCheckCanceled();

    void lowFreeSpaceWarning(QnLowFreeSpaceWarning& lowFreeSpaceWarning);

private:
    void startUpdate(const QnUpdateTarget &target);
    void checkForUpdates(
        const QnUpdateTarget& target,
        std::function<void(const QnCheckForUpdateResult& result)> callback = nullptr);

    void setStage(QnFullUpdateStage stage);
    void setStageProgress(int progress);

    void setPeerStage(const QnUuid &peerId, QnPeerUpdateStage stage);
    void setPeerStageProgress(const QnUuid &peerId, QnPeerUpdateStage stage, int progress);

    void finishUpdate(const QnUpdateResult &result);

private:
    QnFullUpdateStage m_stage;

    QPointer<QnCheckForUpdatesPeerTask> m_checkUpdatesTask;
    QPointer<QnUpdateProcess> m_updateProcess;

    QnMediaServerResourceList m_targets;

    bool m_enableClientUpdates;
};
