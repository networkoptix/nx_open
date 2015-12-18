#include "json_rest_result.h"

#include <utils/common/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnRestResult::Error,
                                          (QnRestResult::NoError,               "OK")
                                          (QnRestResult::MissingParameter,      "Required parameter is missing")
                                          (QnRestResult::InvalidParameter,      "Invalid parameter value")
                                          (QnRestResult::CantProcessRequest,    "Internal server error")
                                          (QnRestResult::Forbidden,             "Access denied")
                                          );

QN_FUSION_DEFINE_FUNCTIONS(QnRestResult::Error, (numeric))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnRestResult, (ubjson)(json)(xml)(csv_record), QnRestResult_Fields, (optional, true) )
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QN_REST_RESULT_TYPES, (ubjson)(json), _Fields, (optional, true))

QnRestResult::QnRestResult():
    error(NoError)
{}

void QnRestResult::setError(Error errorValue, const QString &errorStringValue) {
    error = errorValue;
    errorString = errorStringValue;
}

QnJsonRestResult::QnJsonRestResult() :
    QnRestResult()
{}

QnJsonRestResult::QnJsonRestResult(const QnRestResult& base) :
    QnRestResult(base)
{}

QnUbjsonRestResult::QnUbjsonRestResult() :
    QnRestResult()
{}

QnUbjsonRestResult::QnUbjsonRestResult(const QnRestResult& base) :
    QnRestResult(base)
{}


/* Dummy methods to make fusion macro compile for json and ubjson at once.  */
void serialize(const QJsonValue &value, QnUbjsonWriter<QByteArray> *stream) {
    Q_ASSERT_X(false, Q_FUNC_INFO, "We should not serialize QJsonValue to UBJson.");
    QN_UNUSED(value, stream);
    return;
}

bool deserialize(QnUbjsonReader<QByteArray> *stream, QJsonValue *target) {
    Q_ASSERT_X(false, Q_FUNC_INFO, "We should not serialize QJsonValue to UBJson.");
    QN_UNUSED(stream, target);
    return true;
}

