// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <ui_general_system_administration_widget.h>

#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/system_administration/widgets/abstract_system_settings_widget.h>

class QPushButton;

class QnSystemSettingsWidget;

class QnGeneralSystemAdministrationWidget:
    public nx::vms::client::desktop::AbstractSystemSettingsWidget
{
public:
    QnGeneralSystemAdministrationWidget(
        nx::vms::api::SaveableSystemSettings* editableSystemSettings,
        QWidget* parent = nullptr);

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

    virtual void retranslateUi() override;

protected:
    void setReadOnlyInternal(bool readOnly) override;

private:
    QScopedPointer<Ui::GeneralSystemAdministrationWidget> ui;

    enum Buttons
    {
        kBusinessRulesButton,
        kEventLogButton,
        kCameraListButton,
        kAuditLogButton,
        kBookmarksButton,

        kButtonCount
    };

    using CustomPaintedButton = nx::vms::client::desktop::CustomPainted<QPushButton>;
    QnSystemSettingsWidget* m_systemSettingsWidget = nullptr;
    std::array<CustomPaintedButton*, kButtonCount> m_buttons;
};
