#ifndef QN_NEW_USER_DIALOG_H
#define QN_NEW_USER_DIALOG_H

#include <QtWidgets/QDialog>
#include <QHash>
#include <QtCore/QScopedPointer>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnWorkbenchContext;
class QCheckBox;

namespace Ui {
    class UserSettingsDialog;
}

class QnUserSettingsDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    enum Element {
        Login,
        CurrentPassword,
        Password,
        AccessRights,
        Email,
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

    void setFocusedElement(QString element);

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
    void updatePassword() { updateElement(CurrentPassword); updateElement(Password); }
    void updateAccessRights() { updateElement(AccessRights); }
    void updateEmail() { updateElement(Email); }
    void loadAccessRightsToUi(quint64 rights);

    void updateDependantPermissions();

    void updateAll();
    void updateSizeLimits();
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

    /** Utility function to access checkboxes. */
    bool isCheckboxChecked(quint64 right);

    /** Utility function to access checkboxes. */
    void setCheckboxChecked(quint64 right, bool checked = true);

    /** Utility function to access checkboxes. */
    void setCheckboxEnabled(quint64 right, bool enabled = true);

    QScopedPointer<Ui::UserSettingsDialog> ui;
    QString m_currentPassword;
    QHash<QString, QnResourcePtr> m_userByLogin;
    QnUserResourcePtr m_user;
    bool m_valid[ElementCount];
    QString m_hints[ElementCount];
    ElementFlags m_flags[ElementCount];
    bool m_hasChanges;
    QHash<quint64, QCheckBox*> m_advancedRights;

    /** Status variable to avoid unneeded checks. */
    bool m_inUpdateDependensies;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnUserSettingsDialog::ElementFlags)

#endif // QN_NEW_USER_DIALOG_H
