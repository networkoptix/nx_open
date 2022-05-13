// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/ptz_fwd.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::core {
namespace ptz {
namespace helpers {

NX_VMS_CLIENT_CORE_API bool getSortedPresets(
    const QnAbstractPtzController* controller,
    QnPtzPresetList& presets);

NX_VMS_CLIENT_CORE_API bool getSortedPresets(
    const QnPtzControllerPtr& controller,
    QnPtzPresetList& presets);

NX_VMS_CLIENT_CORE_API QnPtzPresetList sortedPresets(
    const QnResourcePtr& resource,
    QnPtzPresetList presets);

} // namespace helpers
} // namespace ptz
} // namespace nx::vms::client::core
