#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_stream_reader.h>
#include "vivotek_resource.h"

namespace nx::vms::server::plugins {

class VivotekStreamReader: public QnOnvifStreamReader
{
    using base_type = QnOnvifStreamReader;
public:
    VivotekStreamReader(const QnSharedResourcePointer<VivotekResource>& resource);
protected:
    virtual CameraDiagnostics::Result fetchUpdateVideoEncoder(
        CameraInfoParams* outInfo, bool isPrimary, bool isCameraControlRequired,
        const QnLiveStreamParams& params) const override;
};

} // namespace nx::vms::server::plugins

#endif //ENABLE_ONVIF
