#pragma once

#include "layout_settings_dialog_state.h"

#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class LayoutSettingsDialogStateReducer
{
public:
    using State = LayoutSettingsDialogState;

    static State loadLayout(State state, const QnLayoutResourcePtr& layout);

    static State setLocked(State state, bool value);

    static State setBackgroundImageError(State state, const QString& errorText);
    static State clearBackgroundImage(State state);
    static State setBackgroundImageOpacityPercent(State state, int value);
    static State setBackgroundImageWidth(State state, int value);
    static State setBackgroundImageHeight(State state, int value);
    static State setCropToMonitorAspectRatio(State state, bool value);
    static State setKeepImageAspectRatio(State state, bool value);

    static State startDownloading(State state, const QString& targetPath);
    static State imageDownloaded(State state);
    static State imageSelected(State state, const QString& filename);
    static State startUploading(State state);
    static State imageUploaded(State state, const QString& filename);
    static State setPreview(State state, const QImage& image);
};

} // namespace desktop
} // namespace client
} // namespace nx
