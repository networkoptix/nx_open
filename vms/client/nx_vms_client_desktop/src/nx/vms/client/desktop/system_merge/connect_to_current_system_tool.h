#pragma once

#include <utils/common/id.h>
#include <utils/common/connective.h>
#include <utils/merge_systems_common.h>
#include <nx/utils/software_version.h>
#include <ui/workbench/workbench_state_manager.h>

class QnMediaServerUpdateTool;
class QnMergeSystemsTool;

namespace nx::vms::client::desktop {

class ConnectToCurrentSystemTool: public Connective<QObject>, public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = Connective<QObject>;
    using MergeSystemsStatusValue = ::utils::MergeSystemsStatus::Value;

public:
    enum ErrorCode
    {
        NoError,
        MergeFailed,
        UpdateFailed,
        Canceled
    };

    ConnectToCurrentSystemTool(QObject* parent = nullptr);
    virtual ~ConnectToCurrentSystemTool() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    void start(const QnUuid& targetId, const QString& adminPassword);

    MergeSystemsStatusValue mergeError() const;
    QString mergeErrorMessage() const;
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
    MergeSystemsStatusValue m_mergeError = MergeSystemsStatusValue::ok;
    QString m_mergeErrorMessage;
};

} // namespace nx::vms::client::desktop
