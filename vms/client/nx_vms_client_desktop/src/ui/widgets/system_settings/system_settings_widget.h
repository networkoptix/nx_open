// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

struct QnWatermarkSettings;

namespace Ui { class SystemSettingsWidget; }

class QnSystemSettingsWidget:
    public QnAbstractPreferencesWidget,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnSystemSettingsWidget(QWidget *parent = nullptr);
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
