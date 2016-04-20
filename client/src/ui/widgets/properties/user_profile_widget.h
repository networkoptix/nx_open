#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserProfileWidget;
}

class QnUserProfileWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnUserProfileWidget(QWidget* parent = 0);
    virtual ~QnUserProfileWidget();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr &user);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    /** Custom access rights are selected */
    bool isCustomAccessRights() const;
private:
    void updateLogin();
    void updatePassword();
    void updateEmail();

private:
    /* Mode of the dialog. */
    enum class Mode
    {
        /* No user is provided. */
        Invalid,
        /* Admin creates a new user. */
        NewUser,
        /* User edits himself. */
        OwnUser,
        /* Admin edits other user. */
        OtherUser
    };

    void updateControlsAccess();
    void updateAccessRightsPresets();

    Qn::GlobalPermissions selectedPermissions() const;
    QnUuid selectedUserGroup() const;
private:
    QScopedPointer<Ui::UserProfileWidget> ui;
    Mode m_mode;
    QnUserResourcePtr m_user;
};
