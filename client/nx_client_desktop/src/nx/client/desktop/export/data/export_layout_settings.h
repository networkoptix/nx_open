#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/core/watermark/watermark.h>

#include <recording/time_period.h>

#include <nx/client/desktop/common/utils/filesystem.h>
#include <core/resource/camera_bookmark.h>

namespace nx {
namespace client {
namespace desktop {

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
};

} // namespace desktop
} // namespace client
} // namespace nx
