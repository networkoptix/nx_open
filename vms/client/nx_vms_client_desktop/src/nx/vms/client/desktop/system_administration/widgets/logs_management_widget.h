// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <utils/common/connective.h>

namespace Ui { class LogsManagementWidget; }

namespace nx::vms::client::desktop {

class LogsManagementWidget:
    public Connective<QnAbstractPreferencesWidget>,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = Connective<QnAbstractPreferencesWidget>;

public:
    explicit LogsManagementWidget(QWidget* parent = nullptr);
    virtual ~LogsManagementWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    void setupUi();

private:
    QScopedPointer<Ui::LogsManagementWidget> ui;
};

} // namespace nx::vms::client::desktop
