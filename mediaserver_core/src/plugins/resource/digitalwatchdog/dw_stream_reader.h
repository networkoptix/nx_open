#pragma once

#include <plugins/resource/onvif/onvif_stream_reader.h>

class QnDWStreamReader : public QnOnvifStreamReader
{
public:
    QnDWStreamReader(const QnResourcePtr& res);

protected:
    virtual void postStreamConfigureHook() override;

};


