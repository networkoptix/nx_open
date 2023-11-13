// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/merge_status_reply.h>
#include <nx/vms/api/data/module_information.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/merge_systems_common.h>

namespace Ui { class MergeSystemsDialog; }

namespace nx::vms::client::core {
class AbstractRemoteConnectionUserInteractionDelegate;
} // namespace nx::vms::client::core

namespace nx::vms::client::desktop {

class MergeSystemsTool;

class MergeSystemsDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    using Delegate = nx::vms::client::core::AbstractRemoteConnectionUserInteractionDelegate;

public:
    explicit MergeSystemsDialog(QWidget* parent, std::unique_ptr<Delegate> delegate);
    virtual ~MergeSystemsDialog() override;

    nx::utils::Url url() const;
    QString password() const;

private slots:
    void at_urlComboBox_activated(int index);
    void at_urlComboBox_editingFinished();
    void at_testConnectionButton_clicked();
    void at_mergeButton_clicked();

    void at_mergeTool_systemFound(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation,
        const nx::vms::api::MergeStatusReply& reply);

    void at_mergeTool_mergeFinished(
        MergeSystemsStatus mergeStatus,
        const QString& errorText,
        const nx::vms::api::ModuleInformation& moduleInformation);

private:
    void updateKnownSystems();
    void updateErrorLabel(const QString& error);
    void updateConfigurationBlock();

private:
    QScopedPointer<Ui::MergeSystemsDialog> ui;
    QPushButton* m_mergeButton = nullptr;

    std::unique_ptr<Delegate> m_delegate;
    MergeSystemsTool* const m_mergeTool;

    QnUuid m_mergeContextId;
    std::optional<nx::vms::api::ModuleInformation> m_targetModule;
    nx::utils::Url m_url;
    nx::network::http::Credentials m_remoteOwnerCredentials;
};

} //namespace nx::vms::client::desktop
