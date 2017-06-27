#pragma once

#include <functional>

#include <QtCore/QScopedPointer>
#include <QtGui/QValidator>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/utils/validators.h>

class QnUserRolesModel;
class QnUuid;

namespace Ui {
class SubjectSelectionDialog;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class SubjectSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit SubjectSelectionDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);
    virtual ~SubjectSelectionDialog() override;

    void showAlert(const QString& text);

    void setUserValidator(Qn::UserValidator userValidator);

    // Invalid ids are filtered out. Disabled users are kept, but hidden.
    void setCheckedSubjects(const QSet<QnUuid>& ids);

    // Explicitly checked subjects, regardless of allUsers value.
    QSet<QnUuid> checkedSubjects() const;

    // Explicitly checked users + users from checked roles.
    // If allUsers is set, returns all enabled users in the system.
    // Used mostly for entire selection validation and calculating alert text.
    QSet<QnUuid> totalCheckedUsers() const;

    bool allUsers() const; //< Whether all possible users are selected.
    void setAllUsers(bool value);

    bool allUsersSelectorEnabled() const;
    void setAllUsersSelectorEnabled(bool value);

signals:
    void changed(); //< Selection or contents were changed. Potential alert must be re-evaluated.

private:
    void showAllUsersChanged(bool value);
    void validateAllUsers();

private:
    class UserListModel;
    class RoleListModel;
    class UserListDelegate;
    class RoleListDelegate;

private:
    QScopedPointer<Ui::SubjectSelectionDialog> ui;
    RoleListModel* m_roles = nullptr;
    UserListModel* m_users = nullptr;
    RoleListDelegate* m_roleListDelegate = nullptr;
    UserListDelegate* m_userListDelegate = nullptr;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
