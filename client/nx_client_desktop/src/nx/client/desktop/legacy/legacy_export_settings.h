#pragma once

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>

namespace nx {
namespace client {
namespace desktop {
namespace legacy {

struct LegacyExportMediaSettings
{
    QnMediaResourcePtr mediaResource;
    QnTimePeriod timePeriod;
    QString fileName;
    QnLegacyTranscodingSettings imageParameters;
    qint64 serverTimeZoneMs = 0;
    qint64 timelapseFrameStepMs = 0; //< 0 means disabled timelapse.
};

struct LegacyExportLayoutSettings
{
    enum class Mode
    {
        LocalSave,      //< Save over existing file.
        LocalSaveAs,    //< Save to other location.
        Export          //< Add new exported layout.
    };

    LegacyExportLayoutSettings() = default;
    LegacyExportLayoutSettings(QnLayoutResourcePtr layout,
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

} // namespace legacy
} // namespace desktop
} // namespace client
} // namespace nx
