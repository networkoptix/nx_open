// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_settings_dialog_state.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;

QDebug& operator<<(QDebug& dbg, const State::BackgroundImageStatus& value)
{
    dbg << nx::reflect::enumeration::toString(value).c_str();
    return dbg.space();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LayoutSettingsDialogState::Range, (debug),
    LayoutSettingsDialogState_Range_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LayoutSettingsDialogState::Background, (debug),
    LayoutSettingsDialogState_Background_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LayoutSettingsDialogState, (debug),
    LayoutSettingsDialogState_Fields)

} // namespace nx::vms::client::desktop
