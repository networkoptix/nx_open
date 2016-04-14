#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserSettingsWidget;
}

class QnUserSettingsWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnUserSettingsWidget(QWidget* parent = 0);
    virtual ~QnUserSettingsWidget();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr &user);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

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

    void createAccessRightsAdvanced();
    QCheckBox *createAccessRightCheckBox(QString text, int right, QWidget *previous);
    void selectAccessRightsPreset(int rights);
    void fillAccessRightsAdvanced(int rights);

private:
    QScopedPointer<Ui::UserSettingsWidget> ui;
    Mode m_mode;
    QnUserResourcePtr m_user;
};
