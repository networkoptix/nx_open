#pragma once

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>

namespace nx {
namespace client {
namespace desktop {

struct LayoutExportSettings
{
    enum class Mode
    {
        LocalSave,      //< Save over existing file.
        LocalSaveAs,    //< Save to other location.
        Export          //< Add new exported layout.
    };

    LayoutExportSettings() = default;
    LayoutExportSettings(QnLayoutResourcePtr layout,
        QnTimePeriod period,
        QString filename,
        Mode mode,
        bool readOnly)
        :
        layout(layout),
        period(period),
        filename(filename),
        mode(mode),
        readOnly(readOnly)
    {}

    QnLayoutResourcePtr layout; //< Layout that should be exported.
    QnTimePeriod period;        //< Time period to export.
    QString filename;           //< Target filename.
    Mode mode = Mode::Export;   //< Export mode.
    bool readOnly = false;      //< Make exported layout read-only.
};

} // namespace desktop
} // namespace client
} // namespace nx
