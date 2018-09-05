#include "commands.h"

#include "ipc/named_pipe_socket.h"

#include <nx/utils/log/log.h>

namespace applauncher {
namespace api {

ResultType::Value sendCommandToApplauncher(
    const BaseTask& commandToSend,
    Response* const response,
    int timeoutMs)
{
    NamedPipeSocket sock;
    SystemError::ErrorCode resultCode = sock.connectToServerSync(kLauncherPipeName);
    if (resultCode != SystemError::noError)
    {
        NX_LOG( lit("Failed to connect to local server %1. %2").arg(kLauncherPipeName).arg(
            SystemError::toString(resultCode)), cl_logWARNING );
        return ResultType::connectError;
    }

    const QByteArray& serializedTask = commandToSend.serialize();
    unsigned int bytesWritten = 0;
    resultCode = sock.write(serializedTask.data(), serializedTask.size(), &bytesWritten);
    if (resultCode != SystemError::noError)
    {
        NX_LOG( lit("Failed to send launch task to local server %1. %2").arg(kLauncherPipeName
        ).arg(SystemError::toString(resultCode)), cl_logWARNING );
        return ResultType::connectError;
    }

    char buf[kMaxMessageLengthBytes];
    unsigned int bytesRead = 0;
    resultCode = sock.read(buf, sizeof(buf), &bytesRead, timeoutMs); //ignoring return code
    if (resultCode != SystemError::noError)
    {
        NX_LOG( lit("Failed to read response from local server %1. %2").arg(kLauncherPipeName)
            .arg(SystemError::toString(resultCode)), cl_logWARNING );
        return ResultType::connectError;
    }

    if (response)
    {
        if (!response->deserialize(QByteArray::fromRawData(buf, bytesRead)))
            return ResultType::badResponse;
    }

    return ResultType::ok;
}

} // namespace api
} // namespace applauncher
