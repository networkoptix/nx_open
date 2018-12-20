#pragma once

#include <client_core/connection_context_aware.h>

#include <ui/dialogs/common/dialog.h>
#include <utils/common/credentials.h>

class QnSetupWizardDialogPrivate;
class QnSetupWizardDialog: public QnDialog, public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = QnDialog;

public:
    explicit QnSetupWizardDialog(QWidget* parent = nullptr);
    ~QnSetupWizardDialog() override;

    virtual int exec() override;

    QUrl url() const;
    void setUrl(const QUrl& url);

    void loadPage();

    nx::vms::common::Credentials localCredentials() const;
    nx::vms::common::Credentials cloudCredentials() const;
    void setCloudCredentials(const nx::vms::common::Credentials& value);

    bool savePassword() const;

private:
    Q_DECLARE_PRIVATE(QnSetupWizardDialog)
    QScopedPointer<QnSetupWizardDialogPrivate> d_ptr;
};
