#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_stream_reader.h>

class QnDwStreamReader: public QnOnvifStreamReader
{
    using base_type = QnOnvifStreamReader;
public:
    QnDwStreamReader(const QnResourcePtr& res): base_type(res) {}
protected:
    virtual CameraDiagnostics::Result openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params) override;
};

#endif //ENABLE_ONVIF
