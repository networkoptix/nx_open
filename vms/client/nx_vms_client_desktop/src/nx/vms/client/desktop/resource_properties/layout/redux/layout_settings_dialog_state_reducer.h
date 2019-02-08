#pragma once

#include "layout_settings_dialog_state.h"

#include <core/resource/resource_fwd.h>

#include <utils/common/aspect_ratio.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API LayoutSettingsDialogStateReducer
{
public:
    /** Debug-level state tracing. */
    static bool isTracingEnabled();

    /** Enable debug-level state tracing. */
    static void setTracingEnabled(bool value);

    static QnAspectRatio screenAspectRatio();
    static void setScreenAspectRatio(QnAspectRatio value);

    static bool keepBackgroundAspectRatio();
    static void setKeepBackgroundAspectRatio(bool value);

    using State = LayoutSettingsDialogState;

    static State loadLayout(State state, const QnLayoutResourcePtr& layout);

    static State setLocked(State state, bool value);
    static State setLogicalId(State state, int value);
    static State resetLogicalId(State state);
    static State generateLogicalId(State state);
    static State setFixedSizeEnabled(State state, bool value);
    static State setFixedSizeWidth(State state, int value);
    static State setFixedSizeHeight(State state, int value);

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

} // namespace nx::vms::client::desktop
