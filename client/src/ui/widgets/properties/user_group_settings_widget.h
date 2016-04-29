#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserGroupSettingsWidget;
}

class QnUserGroupSettingsModel;

class QnUserGroupSettingsWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnUserGroupSettingsWidget(QnUserGroupSettingsModel* model, QWidget* parent = 0);
    virtual ~QnUserGroupSettingsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    QScopedPointer<Ui::UserGroupSettingsWidget> ui;
    QnUserGroupSettingsModel* m_model;
};
