// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "commands.h"

#include <nx/build_info.h>
#include <nx/utils/ipc/named_pipe_socket.h>
#include <nx/utils/log/log.h>

namespace nx::vms::applauncher::api {

namespace {

constexpr int kMaxMessageLengthBytes = 1024 * 64;

} // namespace

ResultType sendCommandToApplauncher(
    const BaseTask& commandToSend,
    Response* const response,
    int timeoutMs)
{
    nx::utils::NamedPipeSocket sock;
    const QString pipeName = launcherPipeName();
    SystemError::ErrorCode resultCode = sock.connectToServerSync(pipeName.toStdString());
    if (resultCode != SystemError::noError)
    {
        // Client often fails to find applauncher if it is running from build directory, so don't
        // spam logs in this case.
        if (nx::build_info::publicationType() != nx::build_info::PublicationType::local)
        {
            NX_WARNING(NX_SCOPE_TAG, "%1: Failed to connect to pipe %2. %3", __func__, pipeName,
                SystemError::toString(resultCode));
        }

        return ResultType::connectError;
    }

    const QByteArray& serializedTask = commandToSend.serialize();
    unsigned int bytesWritten = 0;
    resultCode = sock.write(serializedTask.data(), serializedTask.size(), &bytesWritten);
    if (resultCode != SystemError::noError)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "%1: Failed to send launch task to local server %2. %3",
            __func__,
            launcherPipeName(),
            SystemError::toString(resultCode));
        return ResultType::connectError;
    }

    char buf[kMaxMessageLengthBytes];
    unsigned int bytesRead = 0;
    resultCode = sock.read(buf, sizeof(buf), &bytesRead, timeoutMs);
    if (resultCode != SystemError::noError)
    {
        NX_WARNING(NX_SCOPE_TAG, "%1: Failed to read response from local server %2. %3",
            __func__,
            launcherPipeName(),
            SystemError::toString(resultCode));
        return ResultType::connectError;
    }

    if (response)
    {
        if (!response->deserialize(QByteArray::fromRawData(buf, bytesRead)))
            return ResultType::badResponse;
    }

    return ResultType::ok;
}

} // namespace nx::vms::applauncher::api
