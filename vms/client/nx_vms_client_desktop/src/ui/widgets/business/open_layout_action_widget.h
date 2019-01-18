#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/subject_target_action_widget.h>

namespace Ui {
class OpenLayoutActionWidget;
} // namespace Ui

class QnResourceListModel;

namespace nx::vms::client::desktop {

/**
 * A widget to edit parameters for openLayoutAction
 * Provides UI to:
 *  - set layout to be opened
 *  - set user/role list
 */
class OpenLayoutActionWidget : public QnSubjectTargetActionWidget
{
    Q_OBJECT
    using base_type = QnSubjectTargetActionWidget;

public:
    explicit OpenLayoutActionWidget(QWidget* parent = nullptr);
    virtual ~OpenLayoutActionWidget() override;

    virtual void updateTabOrder(QWidget* before, QWidget* after) override;

protected:
    virtual void at_model_dataChanged(Fields fields) override;

private:
    // Warnings for picked layout.
    // They are displayed under 'select layout' button
    enum class LayoutWarning
    {
        NoWarning,
        MissingAccess,  //< Some users have no access
        NobodyHasAccess,   //< No user has an access
    };

    // Warnings for picked users.
    // They are displayed under 'select user' button.
    enum class UserWarning
    {
        NoWarning,
        LocalResource,  //< Selected local resource and multiple users
        EmptyRoles  //< Selected some roles, but there are no users in them
    };

    void displayWarning(LayoutWarning warning);
    void displayWarning(UserWarning warning);

    // Analyses current choice and picks proper warnings.
    void checkWarnings();

    // Get selected layout.
    // Can return nullptr.
    QnLayoutResourcePtr getSelectedLayout();

    QnUserResourceList getSelectedUsers(bool& rolesSelected);

    // Updates button state according to selected layouts.
    void updateLayoutsButton();
    // Opens a dialog to select necessary layout.
    void openLayoutSelectionDialog();

private:
    LayoutWarning m_layoutWarning = LayoutWarning::NoWarning;
    UserWarning m_userWarning = UserWarning::NoWarning;

    QScopedPointer<Ui::OpenLayoutActionWidget> ui;

    QnLayoutResourcePtr m_selectedLayout;
};

} // namespace nx::vms::client::desktop
