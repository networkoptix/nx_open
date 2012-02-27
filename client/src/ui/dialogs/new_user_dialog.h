#ifndef QN_NEW_USER_DIALOG_H
#define QN_NEW_USER_DIALOG_H

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
    class NewUserDialog;
}

class QnNewUserDialog: public QDialog {
    Q_OBJECT;
public:
    QnNewUserDialog(QWidget *parent = NULL);

    virtual ~QnNewUserDialog();

    QString login() const;

    QString password() const;

    bool isAdmin() const;

protected:
    enum Element {
        Login,
        Password,
        AccessRights,
        ElementCount
    };

    void setValid(Element element, bool valid);
    void setHint(Element element, const QString &hint);

protected slots:
    void updateElement(Element element);
    void updateLogin() { updateElement(Login); }
    void updatePassword() { updateElement(Password); }
    void updateAccessRights() { updateElement(AccessRights); }

    void updateAll();

private:
    QScopedPointer<Ui::NewUserDialog> ui;
    bool m_valid[ElementCount];
    QString m_hints[ElementCount];
};

#endif // QN_NEW_USER_DIALOG_H
