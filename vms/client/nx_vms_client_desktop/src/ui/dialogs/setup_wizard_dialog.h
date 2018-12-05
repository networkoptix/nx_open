#pragma once

#include <client_core/connection_context_aware.h>
#include <nx/vms/client/core/common/utils/encoded_credentials.h>

#include <ui/dialogs/common/dialog.h>

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

    QnEncodedCredentials localCredentials() const;
    QnEncodedCredentials cloudCredentials() const;
    void setCloudCredentials(const QnEncodedCredentials& value);

    bool savePassword() const;

private:
    Q_DECLARE_PRIVATE(QnSetupWizardDialog)
    QScopedPointer<QnSetupWizardDialogPrivate> d_ptr;
};
