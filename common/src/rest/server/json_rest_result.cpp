#include "json_rest_result.h"

#include <utils/common/model_functions.h>

QnRestResult::QnRestResult(): 
    error(NoError) 
{}

void QnRestResult::setError(Error error, const QString &errorText) {
    error = error;
    errorString = errorText.toLatin1();
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

QN_FUSION_DEFINE_FUNCTIONS(QnRestResult::Error, (numeric))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(QN_REST_RESULT_TYPES, (ubjson)(json), _Fields, (optional, true))
