#pragma once

#include "integrations_fwd.h"

namespace nx::vms::client::desktop::integrations {

/**
 * Create and register all integration instances in the special storage, which will own them.
 * Storage itself will be created as a singleton with the provided parent.
 */
void initialize(QObject* storageParent);

/**
 * Register custom menu actions, provided by the integrations.
 */
void registerActions(ui::action::MenuFactory* factory);

} // namespace nx::vms::client::desktop::integrations
