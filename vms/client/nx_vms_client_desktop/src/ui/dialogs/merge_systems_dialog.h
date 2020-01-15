#pragma once

#include <QtWidgets/QDialog>
#include <QtNetwork/QAuthenticator>

#include <core/resource/resource_fwd.h>
#include <utils/merge_systems_common.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_state_manager.h>

#include <nx/utils/url.h>
#include <nx/vms/api/data_fwd.h>

namespace Ui { class QnMergeSystemsDialog; }

class QnMergeSystemsTool;

class QnMergeSystemsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit QnMergeSystemsDialog(QWidget* parent);
    virtual ~QnMergeSystemsDialog() override;

    nx::utils::Url url() const;
    QString password() const;

private slots:
    void at_urlComboBox_activated(int index);
    void at_urlComboBox_editingFinished();
    void at_testConnectionButton_clicked();
    void at_mergeButton_clicked();

    void at_mergeTool_systemFound(
        utils::MergeSystemsStatus::Value mergeStatus,
        const nx::vms::api::ModuleInformation& moduleInformation,
        const QnMediaServerResourcePtr& discoverer);

    void at_mergeTool_mergeFinished(
        utils::MergeSystemsStatus::Value mergeStatus,
        const nx::vms::api::ModuleInformation& moduleInformation);

private:
    void updateKnownSystems();
    void updateErrorLabel(const QString &error);
    void updateConfigurationBlock();

private:
    QScopedPointer<Ui::QnMergeSystemsDialog> ui;
    QPushButton* m_mergeButton = nullptr;

    QnMergeSystemsTool* const m_mergeTool;

    QnMediaServerResourcePtr m_discoverer;
    nx::utils::Url m_url;
    QAuthenticator m_remoteOwnerCredentials;
};
