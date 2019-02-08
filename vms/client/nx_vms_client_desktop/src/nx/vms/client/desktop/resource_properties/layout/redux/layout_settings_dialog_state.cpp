#include "layout_settings_dialog_state.h"

#include <ostream>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {

using State = LayoutSettingsDialogState;

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(LayoutSettingsDialogState, BackgroundImageStatus,
    (LayoutSettingsDialogState::BackgroundImageStatus::empty, "empty")
    (LayoutSettingsDialogState::BackgroundImageStatus::error, "error")
    (LayoutSettingsDialogState::BackgroundImageStatus::downloading, "downloading")
    (LayoutSettingsDialogState::BackgroundImageStatus::loading, "loading")
    (LayoutSettingsDialogState::BackgroundImageStatus::loaded, "loaded")
    (LayoutSettingsDialogState::BackgroundImageStatus::newImageLoading, "newImageLoading")
    (LayoutSettingsDialogState::BackgroundImageStatus::newImageLoaded, "newImageLoaded")
    (LayoutSettingsDialogState::BackgroundImageStatus::newImageUploading, "newImageUploading")
)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LayoutSettingsDialogState::Range, (debug)(eq),
    LayoutSettingsDialogState_Range_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LayoutSettingsDialogState::Background, (debug)(eq),
    LayoutSettingsDialogState_Background_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LayoutSettingsDialogState, (debug)(eq),
    LayoutSettingsDialogState_Fields)

QDebug& operator<<(QDebug& dbg, const State::BackgroundImageStatus& value)
{
    dbg << QnLexical::serialized(value);
    return dbg.space();
}

} // namespace nx::vms::client::desktop
