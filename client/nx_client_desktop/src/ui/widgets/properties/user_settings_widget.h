#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

namespace Ui
{
    class UserSettingsWidget;
}

class QnUserSettingsModel;
class QnUserRolesModel;
class QnInputField;
class QnAligner;

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
    QList<QnInputField*> inputFields() const;
    QList<QnInputField*> localInputFields() const;
    QList<QnInputField*> cloudInputFields() const;

    QList<QnInputField*> relevantInputFields() const;

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
    QList<QnInputField*> m_localInputFields;
    QList<QnInputField*> m_cloudInputFields;
    QnAligner* const m_aligner;
    int m_lastUserTypeIndex;
};
