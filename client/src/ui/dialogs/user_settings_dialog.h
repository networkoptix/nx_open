#ifndef QN_NEW_USER_DIALOG_H
#define QN_NEW_USER_DIALOG_H

#include <QDialog>
#include <QScopedPointer>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class UserSettingsDialog;
}

class QnUserSettingsDialog: public QDialog {
    Q_OBJECT;
public:
    enum Element {
        Login,
        Password,
        AccessRights,
        ElementCount
    };

    enum ElementFlag {
        Visible = 0x1,
        Editable = 0x2
    };
    Q_DECLARE_FLAGS(ElementFlags, ElementFlag)

    QnUserSettingsDialog(QWidget *parent = NULL);
    virtual ~QnUserSettingsDialog();

    const QnUserResourcePtr &user() const;
    void setUser(const QnUserResourcePtr &user);

    void updateFromResource();
    void submitToResource();

    void setElementFlags(Element element, ElementFlags flags);
    ElementFlags elementFlags(Element element) const;

    bool hasChanges() const {
        return m_hasChanges;
    }

signals:
    void hasChangesChanged();

protected:
    void setValid(Element element, bool valid);
    void setHint(Element element, const QString &hint);

protected slots:
    void updateElement(Element element);
    void updateLogin() { updateElement(Login); }
    void updatePassword() { updateElement(Password); }
    void updateAccessRights() { updateElement(AccessRights); }

    void updateAll();
    void setHasChanges(bool hasChanges = true);

private:
    QScopedPointer<Ui::UserSettingsDialog> ui;
    QnUserResourcePtr m_user;
    bool m_valid[ElementCount];
    QString m_hints[ElementCount];
    ElementFlags m_flags[ElementCount];
    bool m_hasChanges;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnUserSettingsDialog::ElementFlags);

#endif // QN_NEW_USER_DIALOG_H
