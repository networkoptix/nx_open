#pragma once

#include <QtWidgets/QDialog>

#include <utils/common/credentials.h>

class QnSetupWizardDialogPrivate;
class QnSetupWizardDialog : public QDialog
{
    Q_OBJECT

    typedef QDialog base_type;

public:
    explicit QnSetupWizardDialog(QWidget *parent = nullptr);
    ~QnSetupWizardDialog();

    virtual int exec() override;

    QUrl url() const;
    void setUrl(const QUrl& url);

    QnCredentials localCredentials() const;
    QnCredentials cloudCredentials() const;
    void setCloudCredentials(const QnCredentials& value);
private:
    Q_DECLARE_PRIVATE(QnSetupWizardDialog);
    QScopedPointer<QnSetupWizardDialogPrivate> d_ptr;
};
