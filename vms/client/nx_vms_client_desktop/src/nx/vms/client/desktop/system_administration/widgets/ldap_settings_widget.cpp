// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ldap_settings_widget.h"

namespace nx::vms::client::desktop {

struct LdapSettingsWidget::Private
{
    LdapSettingsWidget* const q;
};

LdapSettingsWidget::LdapSettingsWidget(QWidget* parent):
    base_type(parent),
    d(new Private{.q = this})
{
}

LdapSettingsWidget::~LdapSettingsWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void LdapSettingsWidget::loadDataToUi()
{
    // TODO: Implement me!
}

void LdapSettingsWidget::applyChanges()
{
    // TODO: Implement me!
}

bool LdapSettingsWidget::hasChanges() const
{
    // TODO: Implement me!
    return false;
}

} // namespace nx::vms::client::desktop
