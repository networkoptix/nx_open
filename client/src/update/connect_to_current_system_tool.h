#pragma once

#include <QtCore/QObject>

#include <utils/common/id.h>
#include <utils/common/connective.h>
#include <update/updates_common.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

class QnNetworkPeerTask;
class QnMediaServerUpdateTool;
struct QnUpdateResult;

class QnConnectToCurrentSystemTool : public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    enum ErrorCode
    {
        NoError,
        AuthentificationFailed,
        ConfigurationFailed,
        UpdateFailed,
        Canceled
    };

    explicit QnConnectToCurrentSystemTool(QObject *parent = 0);
    ~QnConnectToCurrentSystemTool();

    bool tryClose(bool force);
    void forcedUpdate();

    void start(const QSet<QnUuid> &targets, const QString &adminPassword);

    QSet<QnUuid> targets() const;
    QString adminPassword() const;

public slots:
    void cancel();

signals:
    void finished(int errorCode);
    void progressChanged(int progress);
    void stateChanged(const QString &stateText);

private:
    void finish(ErrorCode errorCode);
    void waitPeers();
    void updatePeers();

private slots:
    void at_configureTask_finished(int errorCode, const QSet<QnUuid> &failedPeers);
    void at_waitTask_finished(int errorCode);
    void at_updateTool_finished(const QnUpdateResult &result);
    void at_updateTool_stageProgressChanged(QnFullUpdateStage stage, int progress);

private:
    QSet<QnUuid> m_targets;
    QString m_user;
    QString m_adminPassword;

    QSet<QnUuid> m_restartTargets;
    QSet<QnUuid> m_updateTargets;
    QHash<QnUuid, QnUuid> m_waitTargets;
    QPointer<QnNetworkPeerTask> m_currentTask;
    QPointer<QnMediaServerUpdateTool> m_updateTool;
    bool m_restartAllPeers;

    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
};
