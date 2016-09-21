/**********************************************************
* Nov 20, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_FUSION_REQUEST_RESULT_H
#define NX_FUSION_REQUEST_RESULT_H

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

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

enum class FusionRequestErrorDetail
{
    ok = 0,
    responseSerializationError,
    deserializationError,
    notAcceptable
};

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

} // namespace nx_http

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx_http::FusionRequestErrorClass)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx_http::FusionRequestErrorClass), (lexical))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx_http::FusionRequestErrorDetail)
//not using QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES here since it does not support declspec
void NX_NETWORK_API serialize(const nx_http::FusionRequestErrorDetail&, QString*);

#endif  //NX_FUSION_REQUEST_RESULT_H
