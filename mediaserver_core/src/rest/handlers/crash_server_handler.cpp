/**********************************************************
* 20 mar 2015
* a.kolesnikov
***********************************************************/

#include "crash_server_handler.h"

#include <nx/utils/crash_dump/systemexcept.h>
#include <nx/utils/log/log.h>
#include <nx/network/http/http_types.h>

int QnCrashServerHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& /*responseMessageBody*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* )
{
    if( !params.contains(lit("razrazraz")) )
        return nx_http::StatusCode::forbidden;

#ifdef _WIN32
    const bool createFullCrashDump = params.contains(lit("full"));
    win32_exception::setCreateFullCrashDump( createFullCrashDump );
#endif

#ifdef __linux__
    const bool createFullCrashDump = params.contains(lit("full"));
    linux_exception::setSignalHandlingDisabled( createFullCrashDump );
#endif

    NX_LOG( lit("Received request to kill server. Killing..."), cl_logALWAYS );
    int* x = nullptr;
    *x = 0;
    return *x;
}

int QnCrashServerHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*requestBody*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*responseMessageBody*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* )
{
    return 0;
}
