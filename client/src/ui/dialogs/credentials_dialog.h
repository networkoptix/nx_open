#ifndef CREDENTIALS_DIALOG_H
#define CREDENTIALS_DIALOG_H

#include <QtWidgets/QDialog>

namespace Ui {
    class QnCredentialsDialog;
}

class QnCredentialsDialog : public QDialog {
    Q_OBJECT
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
