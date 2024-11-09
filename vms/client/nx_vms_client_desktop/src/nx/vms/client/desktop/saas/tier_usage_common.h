// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/saas_data.h>

namespace nx::vms::client::desktop::saas {

/**
 * @return True if given tier limitation does not imply a numerical value.
 */
bool isBooleanTierLimitType(nx::vms::api::SaasTierLimitName limitType);

/**
 * @return Human readable representation of given tier limitation type.
 */
QString tierLimitTypeToString(nx::vms::api::SaasTierLimitName limitType);

} // namespace nx::vms::client::desktop::saas
