#pragma once

#include <utils/common/id.h>
#include <utils/common/connective.h>
#include <utils/merge_systems_common.h>
#include <update/updates_common.h>
#include <ui/workbench/workbench_state_manager.h>

class QnMediaServerUpdateTool;
class QnMergeSystemsTool;

class QnConnectToCurrentSystemTool: public Connective<QObject>, public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    enum ErrorCode
    {
        NoError,
        MergeFailed,
        UpdateFailed,
        Canceled
    };

    QnConnectToCurrentSystemTool(QObject* parent = nullptr);
    virtual ~QnConnectToCurrentSystemTool() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    void start(const QnUuid& targetId, const QString& adminPassword);

    utils::MergeSystemsStatus::Value mergeError() const;
    QString mergeErrorMessage() const;
    QnUpdateResult updateResult() const;
    QnUuid getTargetId() const;
    QnUuid getOriginalId() const;
    bool wasServerIncompatible() const;
    nx::utils::SoftwareVersion getServerVersion() const;
    /** Get name of the server being merged. */
    QString getServerName() const;

public slots:
    void cancel();

signals:
    void finished(int errorCode);
    void progressChanged(int progress);
    void stateChanged(const QString& stateText);

private:
    void finish(ErrorCode errorCode);
    void mergeServer();
    void waitServer();
    void updateServer();

private:
    QnUuid m_targetId;
    QnUuid m_originalTargetId;
    QString m_adminPassword;
    QString m_serverName;
    nx::utils::SoftwareVersion m_serverVersion;
    bool m_wasIncompatible = false;

    QPointer<QnMergeSystemsTool> m_mergeTool;
    //QPointer<QnMediaServerUpdateTool> m_updateTool;
    utils::MergeSystemsStatus::Value m_mergeError = utils::MergeSystemsStatus::ok;
    QString m_mergeErrorMessage;
    QnUpdateResult m_updateResult;
};
