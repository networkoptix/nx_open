/**********************************************************
* Nov 20, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_FUSION_REQUEST_RESULT_H
#define NX_FUSION_REQUEST_RESULT_H

#include <QtCore/QString>

#include <utils/common/model_functions_fwd.h>
#include <utils/fusion/fusion_fwd.h>

#include "../httptypes.h"


namespace nx_http {

enum class FusionRequestErrorClass
{
    noError = 0,
    badRequest,
    unauthorized,
    logicError,
    ioError,
    //!indicates server bug?
    internalError
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FusionRequestErrorClass)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((FusionRequestErrorClass), (lexical))

class NX_NETWORK_API FusionRequestResult
{
public:
    enum ErrorDetail
    {
        ecNoDetail = 0,
        ecResponseSerializationError,
        ecDeserializationError
    };

    FusionRequestErrorClass resultCode;
    int errorDetail;
    QString errorText;

    FusionRequestResult();
    FusionRequestResult(
        FusionRequestErrorClass _errorClass,
        int _errorDetail,
        QString _errorText);

    nx_http::StatusCode::Value httpStatusCode() const;
};


#define FusionRequestResult_Fields (resultCode)(errorDetail)(errorText)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (FusionRequestResult),
    (json))

}

#endif  //NX_FUSION_REQUEST_RESULT_H
