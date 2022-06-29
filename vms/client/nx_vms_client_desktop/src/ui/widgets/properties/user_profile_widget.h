// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>


namespace Ui { class UserProfileWidget; }
namespace nx::vms::client::desktop { class Aligner; }
class QnUserSettingsModel;

class QnUserProfileWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    QnUserProfileWidget(QnUserSettingsModel* model, QWidget* parent = 0);
    virtual ~QnUserProfileWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool canApplyChanges() const override;

    void updatePermissionsLabel(const QString& text);

private:
    QString requestNewPassword(const QString& reason = {});
    void editPassword();
    void handleDigestSupportChange();
    void updateControlsAccess();
    bool validMode() const;

private:
    QScopedPointer<Ui::UserProfileWidget> ui;
    QnUserSettingsModel* const m_model;
    QString m_newPassword;
    nx::vms::client::desktop::Aligner* m_aligner;
};
