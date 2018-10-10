#include "commands.h"

#include "nx/utils/ipc/named_pipe_socket.h"

#include <nx/utils/log/log.h>

namespace applauncher {
namespace api {

namespace {

static const auto kLogTag = nx::utils::log::Tag(QString("applauncher::api"));

} // namespace

ResultType::Value sendCommandToApplauncher(
    const BaseTask& commandToSend,
    Response* const response,
    int timeoutMs)
{
    NamedPipeSocket sock;
    QString pipeName = launcherPipeName();
    SystemError::ErrorCode resultCode = sock.connectToServerSync(pipeName);
    if (resultCode != SystemError::noError)
    {
        NX_WARNING(kLogTag, lm("Failed to connect to local server %1. %2").args(
            pipeName,
            SystemError::toString(resultCode)));
        return ResultType::connectError;
    }

    const QByteArray& serializedTask = commandToSend.serialize();
    unsigned int bytesWritten = 0;
    resultCode = sock.write(serializedTask.data(), serializedTask.size(), &bytesWritten);
    if (resultCode != SystemError::noError)
    {
        NX_WARNING(kLogTag, lm("Failed to send launch task to local server %1. %2").args(
            launcherPipeName(),
            SystemError::toString(resultCode)));
        return ResultType::connectError;
    }

    char buf[kMaxMessageLengthBytes];
    unsigned int bytesRead = 0;
    resultCode = sock.read(buf, sizeof(buf), &bytesRead, timeoutMs);
    if (resultCode != SystemError::noError)
    {
        NX_WARNING(kLogTag, lm("Failed to read response from local server %1. %2").args(
            launcherPipeName(),
            SystemError::toString(resultCode)));
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
