#ifndef CREDENTIALS_DIALOG_H
#define CREDENTIALS_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/dialogs/common/dialog.h>

namespace Ui {
    class QnCredentialsDialog;
}

class QnCredentialsDialog : public QnDialog {
    Q_OBJECT

    typedef QnDialog base_type;

public:
    explicit QnCredentialsDialog(QWidget *parent = 0);
    ~QnCredentialsDialog();

    QString user() const;
    void setUser(const QString &user);

    QString password() const;
    void setPassword(const QString &password);

private:
    QScopedPointer<Ui::QnCredentialsDialog> ui;
};

#endif // CREDENTIALS_DIALOG_H
