#include "server_control_handler.h"

#include <cstdlib>

#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/network/http/http_types.h>
#include <mediaserver_ini.h>

int QnServerControlHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& /*responseMessageBody*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    // NOTE: qWarning() is used to make the messages appear both on stderr and in the log.
    constexpr char messagePrefix[] = "Received /api/serverControl request:";

    ini().reload();
    if (!ini().enableApiServerControl)
    {
        qWarning() << messagePrefix << "Ignoring - not enabled via mediaserver.ini";
        return nx::network::http::StatusCode::forbidden;
    }

    if (params.contains("crash"))
    {
        const bool createFullCrashDump = params.contains("fullDump");
        qWarning() << messagePrefix << "Intentionally crashing the server"
            << (createFullCrashDump ? "with full crash dump" : "");
        #if defined(_WIN32)
            win32_exception::setCreateFullCrashDump(createFullCrashDump);
        #elif defined(__linux__)
            linux_exception::setSignalHandlingDisabled(createFullCrashDump);
        #endif

        int* x = nullptr;
        *x = 0;
        return *x;
    }
    else if (params.contains("exit"))
    {
        qWarning() << messagePrefix << "Exiting the Server via `exit(64)`";
        exit(64);
    }
    else
    {
        qWarning() << messagePrefix << "Ignoring - unsupported params";
        return nx::network::http::StatusCode::forbidden;
    }
}

int QnServerControlHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*requestBody*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*responseMessageBody*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return 0;
}
