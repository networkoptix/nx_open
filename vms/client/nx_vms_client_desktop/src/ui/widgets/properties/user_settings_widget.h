#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

namespace Ui { class UserSettingsWidget; }
namespace nx::vms::client::desktop {

class InputField;
class Aligner;

} // namespace nx::vms::client::desktop

class QnUserSettingsModel;
class QnUserRolesModel;

class QnUserSettingsWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    QnUserSettingsWidget(QnUserSettingsModel* model, QWidget* parent = nullptr);
    virtual ~QnUserSettingsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool canApplyChanges() const override;

    QnUuid selectedUserRoleId() const;
    Qn::UserRole selectedRole() const;

    void updatePermissionsLabel(const QString& text);

signals:
    void userTypeChanged(bool isCloud);

private:
    void setupInputFields();
    QList<nx::vms::client::desktop::InputField*> inputFields() const;
    QList<nx::vms::client::desktop::InputField*> localInputFields() const;
    QList<nx::vms::client::desktop::InputField*> cloudInputFields() const;
    QList<nx::vms::client::desktop::InputField*> relevantInputFields() const;

    QString passwordPlaceholder() const;
    void updatePasswordPlaceholders();

    void updateRoleComboBox();
    void updateControlsAccess();
    bool validMode() const;
    bool mustUpdatePassword() const;

private:
    QScopedPointer<Ui::UserSettingsWidget> ui;
    QnUserSettingsModel* const m_model;
    QnUserRolesModel* const m_rolesModel;
    QList<nx::vms::client::desktop::InputField*> m_localInputFields;
    QList<nx::vms::client::desktop::InputField*> m_cloudInputFields;
    nx::vms::client::desktop::Aligner* const m_aligner;
    int m_lastUserTypeIndex;
};
