#pragma once

#include <QtCore/QObject>

#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/resource/resource_fwd.h>

class CLServerPushStreamReader;

namespace nx::mediaserver::camera {

class ErrorProcessor: public QObject
{
    Q_OBJECT

public:
    enum class Code
    {
        noError,
        frameLost
    };
    Q_ENUM(Code)

    ErrorProcessor(QnMediaServerModule* serverModule);
    void onStreamReaderError(CLServerPushStreamReader* streamReader, Code code);

private:
    void processFrameLostError(CLServerPushStreamReader* streamReader);
};

} // namespace nx::mediaserver::camera