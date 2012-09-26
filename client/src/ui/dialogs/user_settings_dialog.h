#ifndef QN_NEW_USER_DIALOG_H
#define QN_NEW_USER_DIALOG_H

#include <QDialog>
#include <QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchContext;
class QCheckBox;

namespace Ui {
    class UserSettingsDialog;
}

class QnUserSettingsDialog: public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    enum Element {
        Login,
        CurrentPassword,
        Password,
        AccessRights,
        ElementCount
    };

    enum ElementFlag {
        Visible = 0x1,
        Editable = 0x2
    };
    Q_DECLARE_FLAGS(ElementFlags, ElementFlag)

    QnUserSettingsDialog(QnWorkbenchContext *context, QWidget *parent = NULL);
    virtual ~QnUserSettingsDialog();

    const QnUserResourcePtr &user() const;
    void setUser(const QnUserResourcePtr &user);

    void updateFromResource();
    void submitToResource();

    void setElementFlags(Element element, ElementFlags flags);
    ElementFlags elementFlags(Element element) const;

    void setEditorPermissions(quint64 rights);

    bool hasChanges() const {
        return m_hasChanges;
    }

    const QString &currentPassword() const;
    void setCurrentPassword(const QString &currentPassword);

signals:
    void hasChangesChanged();

protected:
    void setValid(Element element, bool valid);
    void setHint(Element element, const QString &hint);

protected slots:
    void updateElement(Element element);
    void updateLogin() { updateElement(Login); }
    void updateCurrentPassword() { updateElement(CurrentPassword); }
    void updatePassword() { updateElement(Password); }
    void updateAccessRights() { updateElement(AccessRights); }
    void loadAccessRightsToUi(quint64 rights);

    void updateAll();
    void setHasChanges(bool hasChanges = true);

    void at_accessRights_changed();
    void at_advancedButton_toggled();

private:
    void createAccessRightsPresets();
    void createAccessRightsAdvanced();
    QCheckBox *createAccessRightCheckBox(QString text, quint64 right, QWidget *previous);
    void selectAccessRightsPreset(quint64 rights);
    void fillAccessRightsAdvanced(quint64 rights);
    quint64 readAccessRightsAdvanced();

    QScopedPointer<Ui::UserSettingsDialog> ui;
    QString m_currentPassword;
    QHash<QString, QnResourcePtr> m_userByLogin;
    QnUserResourcePtr m_user;
    bool m_valid[ElementCount];
    QString m_hints[ElementCount];
    ElementFlags m_flags[ElementCount];
    bool m_hasChanges;
    QHash<quint64, QCheckBox*> m_advancedRights;
    quint64 m_editorRights;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnUserSettingsDialog::ElementFlags)

#endif // QN_NEW_USER_DIALOG_H
