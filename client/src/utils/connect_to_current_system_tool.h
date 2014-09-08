#ifndef CONNECT_TO_CURRENT_SYSTEM_TOOL_H
#define CONNECT_TO_CURRENT_SYSTEM_TOOL_H

#include <QtCore/QObject>

#include <utils/common/id.h>
#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>

class QnConfigurePeerTask;
class QnMediaServerUpdateTool;
class QnWaitCompatibleServersPeerTask;
class QnProgressDialog;

class QnConnectToCurrentSystemTool : public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QObject> base_type;
public:
    enum ErrorCode {
        NoError,
        AuthentificationFailed,
        ConfigurationFailed,
        UpdateFailed
    };

    explicit QnConnectToCurrentSystemTool(QnWorkbenchContext *context, QObject *parent = 0);
    ~QnConnectToCurrentSystemTool();

    void connectToCurrentSystem(const QSet<QUuid> &targets, const QString &password);

    bool isRunning() const;

    QSet<QUuid> targets() const;

signals:
    void finished(int errorCode);

private:
    void finish(ErrorCode errorCode);
    void configureServer();
    void waitPeers();
    void updatePeers();
    void revertApiUrls();

private slots:
    void at_configureTask_finished(int errorCode, const QSet<QUuid> &failedPeers);
    void at_waitTask_finished(int errorCode);
    void at_updateTool_stateChanged(int state);

private:
    bool m_running;
    QSet<QUuid> m_targets;
    QString m_password;

    QSet<QUuid> m_restartTargets;
    QSet<QUuid> m_updateTargets;
    QHash<QUuid, QUrl> m_oldUrls;
    QHash<QUuid, QUuid> m_waitTargets;
    QnConfigurePeerTask *m_configureTask;
    QnWaitCompatibleServersPeerTask *m_waitTask;
    QnMediaServerUpdateTool *m_updateTool;
    QScopedPointer<QnProgressDialog> m_updateProgressDialog;
    int m_prevToolState;
    bool m_updateFailed;
    bool m_restartAllPeers;
};

#endif // CONNECT_TO_CURRENT_SYSTEM_TOOL_H
