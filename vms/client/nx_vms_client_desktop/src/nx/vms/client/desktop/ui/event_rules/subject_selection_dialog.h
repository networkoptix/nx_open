// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <vector>

#include <QtCore/QScopedPointer>
#include <QtGui/QValidator>

#include <business/business_resource_validation.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnUserRolesModel;
class QnResourceAccessSubject;

namespace Ui {
class SubjectSelectionDialog;
} // namespace Ui

namespace nx::vms::client::desktop {
namespace ui {

namespace subject_selection_dialog_private {

class UserListModel;
class RoleListModel;
class UserListDelegate;
class GroupListDelegate;

} //namespace subject_selection_dialog_private

class SubjectSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    struct CustomizableOptions
    {
        // Initializes options with values suitable for Cloud users selection dialog.
        // TODO: #spanasenko Refactor the whole dialog customizaion.
        static CustomizableOptions cloudUsers();

        QString userListHeader;
        std::function<bool(const QnUserResource&)> userFilter;
        bool showAllUsersSwitcher = false;
        QColor alertColor;
    };

public:
    explicit SubjectSelectionDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = {});
    virtual ~SubjectSelectionDialog() override;

    void showAlert(const QString& text);

    void setOptions(const CustomizableOptions& options);

    // Sets user validator. If it's not set, any user is considered valid.
    void setUserValidator(UserValidator userValidator);

    // Sets role validator. If it's not set, role validity is determined by validities of its users.
    void setRoleValidator(RoleValidator roleValidator);

    void setValidationPolicy(QnSubjectValidationPolicy* policy);

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
    std::vector<QnResourceAccessSubject> allSubjects() const;

private:
    QScopedPointer<Ui::SubjectSelectionDialog> ui;

    subject_selection_dialog_private::RoleListModel* m_roles = nullptr;
    subject_selection_dialog_private::UserListModel* m_users = nullptr;
    subject_selection_dialog_private::GroupListDelegate* m_groupListDelegate = nullptr;
    subject_selection_dialog_private::UserListDelegate* m_userListDelegate = nullptr;

    bool m_allUsersSelectorEnabled = true;

    QnSubjectValidationPolicy* m_policy{nullptr};
};

} // namespace ui
} // namespace nx::vms::client::desktop
