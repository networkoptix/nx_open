#pragma once

#include <QtWidgets/QDialog>

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

    QString localLogin() const;
    QString localPassword() const;

    QString cloudLogin() const;
    void setCloudLogin(const QString& login);

    QString cloudPassword() const;
    void setCloudPassword(const QString& password);
private:
    Q_DECLARE_PRIVATE(QnSetupWizardDialog);
    QScopedPointer<QnSetupWizardDialogPrivate> d_ptr;
};
