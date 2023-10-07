// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_administration/widgets/abstract_system_settings_widget.h>

namespace nx::vms::client::desktop {

class OutgoingMailSettingsWidget: public AbstractSystemSettingsWidget
{
public:
    OutgoingMailSettingsWidget(
        api::SaveableSystemSettings* editableSystemSettings, QWidget* parent = nullptr);
    virtual ~OutgoingMailSettingsWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
