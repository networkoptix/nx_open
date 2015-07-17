/**********************************************************
* 16 jul 2014
* a.kolesnikov
***********************************************************/

#include "get_system_name_rest_handler.h"

#include <common/common_module.h>
#include <utils/network/http/httptypes.h>


int QnGetSystemNameRestHandler::executeGet(const QString& /*path*/, const QnRequestParamList& /*params*/, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    result = qnCommon->localSystemName().toUtf8();
    contentType = "application/text";
    return nx_http::StatusCode::ok;
}

int QnGetSystemNameRestHandler::executePost(const QString& /*path*/, const QnRequestParamList& /*params*/, 
                                            const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType */, QByteArray& /*result*/, QByteArray& /*contentType*/
                                            , const QnRestConnectionProcessor*)
{
    return nx_http::StatusCode::forbidden;
}
