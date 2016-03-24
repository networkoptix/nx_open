#pragma once

#include <QtWidgets/QDialog>

class QnSetupWizardDialogPrivate;
class QnSetupWizardDialog : public QDialog
{
    Q_OBJECT

    typedef QDialog base_type;

public:
    explicit QnSetupWizardDialog(const QUrl& serverUrl,  QWidget *parent = nullptr);
    ~QnSetupWizardDialog();

    virtual int exec() override;

private:
    Q_DECLARE_PRIVATE(QnSetupWizardDialog);
    QScopedPointer<QnSetupWizardDialogPrivate> d_ptr;
};
