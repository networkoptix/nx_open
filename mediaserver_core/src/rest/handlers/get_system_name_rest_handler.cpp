/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#include "get_system_name_rest_handler.h"

#include <common/common_module.h>
#include <nx/network/http/httptypes.h>
#include <api/global_settings.h>


int QnGetSystemIdRestHandler::executeGet(const QString& /*path*/, const QnRequestParamList& /*params*/, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    result = qnGlobalSettings->localSystemId().toByteArray();
    contentType = "application/text";
    return nx_http::StatusCode::ok;
}

int QnGetSystemIdRestHandler::executePost(const QString& /*path*/, const QnRequestParamList& /*params*/,
                                            const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType */, QByteArray& /*result*/, QByteArray& /*contentType*/
                                            , const QnRestConnectionProcessor*)
{
    return nx_http::StatusCode::forbidden;
}
