// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/workbench/workbench_state_manager.h>
#include <utils/merge_systems_common.h>

namespace nx::vms::api { struct ModuleInformation; }
namespace nx::vms::client::core { class AbstractRemoteConnectionUserInteractionDelegate; }

class QnMediaServerUpdateTool;

namespace nx::vms::client::desktop {

class MergeSystemsTool;

class ConnectToCurrentSystemTool: public QObject, public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QObject;

public:
    using Delegate = nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate;

public:
    ConnectToCurrentSystemTool(QObject* parent, std::unique_ptr<Delegate> delegate);
    virtual ~ConnectToCurrentSystemTool() override;

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

    void start(const nx::Uuid& targetId);

    /** Get name of the server being merged. */
    QString getServerName() const;

public slots:
    void cancel();

signals:
    void finished(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation);

    void progressChanged(int progress);
    void stateChanged(const QString& stateText);
    void canceled();

private:
    void finish(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation);

    void mergeServer(const QString& adminPassword);
    void waitServer();
    void updateServer();

    bool showLicenseWarning(
        MergeSystemsStatus mergeStatus,
        const nx::vms::api::ModuleInformation& moduleInformation) const;

    QString requestPassword() const;

    void systemFound(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation);

    void mergeFinished(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation);

private:
    nx::Uuid m_targetId;
    nx::Uuid m_originalTargetId;
    QString m_serverName;
    nx::Uuid m_mergeContextId;

    QPointer<MergeSystemsTool> m_mergeTool;
    std::unique_ptr<Delegate> m_delegate;
};

} // namespace nx::vms::client::desktop
