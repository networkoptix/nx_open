#include "json_rest_result.h"

#include <utils/common/model_functions.h>

QnRestResult::QnRestResult(): 
    m_error(NoError) 
{}

const QString &QnRestResult::errorString() const {
    return m_errorString;
}

void QnRestResult::setErrorString(const QString &errorText) {
    m_errorString = errorText;
}

QnRestResult::Error QnRestResult::error() const {
    return m_error;
}

void QnRestResult::setError(Error error) {
    m_error = error;
}

void QnRestResult::setError(Error error, const QString &errorText) {
    m_error = error;
    m_errorString = errorText;
}

QN_FUSION_DEFINE_FUNCTIONS(QnRestResult::Error, (numeric))

QN_FUSION_ADAPT_CLASS_GSN_FUNCTIONS(QnRestResult, 
    (json),
    ((&QnRestResult::m_error,       &QnRestResult::m_error,         "error"))
    ((&QnRestResult::m_errorString, &QnRestResult::m_errorString,   "errorString"))
)

QN_FUSION_ADAPT_CLASS_GSN_FUNCTIONS(QnJsonRestResult, 
(json),
((&QnJsonRestResult::m_reply,       &QnJsonRestResult::m_reply,         "reply"))
)
