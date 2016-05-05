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

enum class FusionRequestErrorDetail
{
    ok = 0,
    responseSerializationError,
    deserializationError,
    notAcceptable
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FusionRequestErrorDetail)
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((FusionRequestErrorDetail), (lexical))

//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
void NX_NETWORK_API serialize(const FusionRequestErrorDetail&, QString*);

class NX_NETWORK_API FusionRequestResult
{
public:
    FusionRequestErrorClass errorClass;
    QString resultCode;
    int errorDetail;
    QString errorText;

    /** Creates OK result */
    FusionRequestResult();
    FusionRequestResult(
        FusionRequestErrorClass _errorClass,
        QString _resultCode,
        int _errorDetail,
        QString _errorText);
    FusionRequestResult(
        FusionRequestErrorClass _errorClass,
        QString _resultCode,
        FusionRequestErrorDetail _errorDetail,
        QString _errorText);

    nx_http::StatusCode::Value httpStatusCode() const;
};


#define FusionRequestResult_Fields (errorClass)(resultCode)(errorDetail)(errorText)

//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
bool NX_NETWORK_API deserialize(QnJsonContext*, const QJsonValue&, class FusionRequestResult*);
void NX_NETWORK_API serialize(QnJsonContext*, const FusionRequestResult&, class QJsonValue*);
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
//    (FusionRequestResult),
//    (json))

}

#endif  //NX_FUSION_REQUEST_RESULT_H
