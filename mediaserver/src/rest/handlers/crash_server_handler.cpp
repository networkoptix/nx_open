/**********************************************************
* 20 mar 2015
* a.kolesnikov
***********************************************************/

#include "crash_server_handler.h"

#ifdef _WIN32
#include <common/systemexcept_win32.h>
#endif
#include <utils/common/log.h>
#include <utils/network/http/httptypes.h>


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
