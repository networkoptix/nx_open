#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/core/watermark/watermark.h>

#include <recording/time_period.h>

#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <core/resource/camera_bookmark.h>
#include <nx/core/transcoding/filters/transcoding_settings.h>

namespace nx::vms::client::desktop {

struct ExportLayoutSettings
{
    enum class Mode
    {
        LocalSave,      //< Save over existing file.
        LocalSaveAs,    //< Save to other location.
        Export          //< Add new exported layout.
    };

    QnLayoutResourcePtr layout; //< Layout that should be exported.
    QnTimePeriod period;        //< Time period to export.
    Filename fileName;           //< Target filename.
    Mode mode = Mode::Export;   //< Export mode.
    bool readOnly = false;      //< Make exported layout read-only.
    nx::core::Watermark watermark;
    QnCameraBookmarkList bookmarks; //< Export only selected bookmarks.

    struct Encryption
    {
        bool on = false;
        QString password;
    } encryption;

    nx::core::transcoding::Settings transcodingSettings;
};

} // namespace nx::vms::client::desktop
