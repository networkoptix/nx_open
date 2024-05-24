// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/uuid.h>

namespace nx::vms::api::combined_id {

NX_VMS_API std::optional<int> findIdSeparator(const QString& id, char separator);
NX_VMS_API nx::Uuid localIdFromCombined(const QString& id);
NX_VMS_API nx::Uuid serverIdFromCombined(const QString& id);
NX_VMS_API nx::Uuid serverIdFromCombined(const QString& id, char endSeparator);
NX_VMS_API QString combineId(const nx::Uuid& localId, const nx::Uuid& serverId);

} // namespace nx::vms::api::combined_id
