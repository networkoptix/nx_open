#pragma once

#include <QtCore/QObject>

#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/resource/resource_fwd.h>
#include "nx/streaming/abstract_media_stream_data_provider.h"

namespace nx::mediaserver::camera {

class ErrorProcessor: public QObject
{
    Q_OBJECT

public:
    ErrorProcessor();
public slots:
    void onStreamReaderEvent(
        QnAbstractMediaStreamDataProvider* streamReader,
        CameraDiagnostics::Result error);
    void processNoError(QnAbstractMediaStreamDataProvider* streamReader);
private:
    void processStreamError(QnAbstractMediaStreamDataProvider* streamReader, CameraDiagnostics::Result error);
};

} // namespace nx::mediaserver::camera