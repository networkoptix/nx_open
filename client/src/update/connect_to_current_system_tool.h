#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>
#include <utils/common/connective.h>
#include <update/updates_common.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

class QnNetworkPeerTask;
class QnMediaServerUpdateTool;
struct QnUpdateResult;

class QnConnectToCurrentSystemTool : public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QObject> base_type;
public:
    enum ErrorCode {
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

    void start(const QSet<QnUuid> &targets, const QString &adminUser, const QString &password);

    QSet<QnUuid> targets() const;
    QString user() const;
    QString password() const;

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
    QString m_password;

    QSet<QnUuid> m_restartTargets;
    QSet<QnUuid> m_updateTargets;
    QHash<QnUuid, QnUuid> m_waitTargets;
    QPointer<QnNetworkPeerTask> m_currentTask;
    QPointer<QnMediaServerUpdateTool> m_updateTool;
    bool m_restartAllPeers;

    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
