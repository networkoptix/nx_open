#include "json_rest_result.h"

QnJsonRestResult::QnJsonRestResult(): 
    m_error(NoError) 
{}

const QString &QnJsonRestResult::errorString() const {
    return m_errorString;
}

void QnJsonRestResult::setErrorString(const QString &errorText) {
    m_errorString = errorText;
}

QnJsonRestResult::Error QnJsonRestResult::error() const {
    return m_error;
}

void QnJsonRestResult::setError(Error error) {
    m_error = error;
}

void QnJsonRestResult::setError(Error error, const QString &errorText) {
    m_error = error;
    m_errorString = errorText;
}

const QJsonValue &QnJsonRestResult::reply() const {
    return m_reply;
}

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QnJsonRestResult, 
    ((&QnJsonRestResult::m_error,       &QnJsonRestResult::m_error,         "error"))
    ((&QnJsonRestResult::m_errorString, &QnJsonRestResult::m_errorString,   "errorString"))
    ((&QnJsonRestResult::m_reply,       &QnJsonRestResult::m_reply,         "reply"))
)

