#pragma once

#include <QtWidgets/QDialog>

#include <client_core/connection_context_aware.h>

#include <ui/dialogs/common/dialog.h>

#include <utils/common/encoded_credentials.h>

class QnSetupWizardDialogPrivate;
class QnSetupWizardDialog : public QnDialog, public QnConnectionContextAware
{
    Q_OBJECT

    typedef QnDialog base_type;

public:
    explicit QnSetupWizardDialog(QWidget *parent = nullptr);
    ~QnSetupWizardDialog();

    virtual int exec() override;

    QUrl url() const;
    void setUrl(const QUrl& url);

    QnEncodedCredentials localCredentials() const;
    QnEncodedCredentials cloudCredentials() const;
    void setCloudCredentials(const QnEncodedCredentials& value);

    bool savePassword() const;
private:
    Q_DECLARE_PRIVATE(QnSetupWizardDialog)
    QScopedPointer<QnSetupWizardDialogPrivate> d_ptr;
};
