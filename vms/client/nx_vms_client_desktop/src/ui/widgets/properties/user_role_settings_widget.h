// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>


namespace Ui
{
class UserRoleSettingsWidget;
}

class QnUserRolesSettingsModel;
class QnUserRolesModel;
class QnUserRoleSettingsWidgetPrivate;

class QnUserRoleSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    QnUserRoleSettingsWidget(QnUserRolesSettingsModel* model, QWidget* parent = 0);
    virtual ~QnUserRoleSettingsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    QScopedPointer<Ui::UserRoleSettingsWidget> ui;

    QnUserRoleSettingsWidgetPrivate* d_ptr;
    Q_DECLARE_PRIVATE(QnUserRoleSettingsWidget);
};
