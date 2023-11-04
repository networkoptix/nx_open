// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/system_administration/widgets/abstract_system_settings_widget.h>

struct QnWatermarkSettings;

namespace Ui { class SystemSettingsWidget; }

class QnSystemSettingsWidget:
    public nx::vms::client::desktop::AbstractSystemSettingsWidget
{
    Q_OBJECT

public:
    QnSystemSettingsWidget(
        nx::vms::api::SaveableSystemSettings* editableSystemSettings,
        QWidget *parent = nullptr);
    virtual ~QnSystemSettingsWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;
    virtual void retranslateUi() override;

    // Used by QnGeneralSystemAdministrationWidget to align widgets.
    QWidget* languageComboBox() const;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    QScopedPointer<Ui::SystemSettingsWidget> ui;
};
