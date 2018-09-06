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
    ErrorProcessor(QnMediaServerModule* serverModule);
public slots:
    void onStreamReaderError(
        QnAbstractMediaStreamDataProvider* streamReader,
        QnAbstractMediaStreamDataProvider::ErrorCode code);
    void processNoError(QnAbstractMediaStreamDataProvider* streamReader);
private:
    void processStreamIssueError(QnAbstractMediaStreamDataProvider* streamReader);
};

} // namespace nx::mediaserver::camera