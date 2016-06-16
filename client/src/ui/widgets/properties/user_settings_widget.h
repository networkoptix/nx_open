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

class QnUserSettingsModel;
class QnInputField;

class QnUserSettingsWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnUserSettingsWidget(QnUserSettingsModel* model, QWidget* parent = 0);
    virtual ~QnUserSettingsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool canApplyChanges() const override;

    QnUuid selectedUserGroup() const;
    Qn::GlobalPermissions selectedPermissions() const;

    void updateAccessRightsPresets();

private:
    void setupInputFields();
    QList<QnInputField*> inputFields() const;

    void updateControlsAccess();
    bool validMode() const;
private:
    QScopedPointer<Ui::UserSettingsWidget> ui;
    QnUserSettingsModel* const m_model;
};
