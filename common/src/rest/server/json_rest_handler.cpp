#include "json_rest_handler.h"

// -------------------------------------------------------------------------- //
// QnJsonRestResult
// -------------------------------------------------------------------------- //
QnJsonRestResult::QnJsonRestResult(): 
    m_errorId(-1) 
{}

const QString &QnJsonRestResult::errorText() const {
    return m_errorText;
}

void QnJsonRestResult::setErrorText(const QString &errorText) {
    m_errorText = errorText;
}

int QnJsonRestResult::errorId() const {
    return m_errorId;
}

void QnJsonRestResult::setErrorId(int errorId) {
    m_errorId = errorId;
}

void QnJsonRestResult::setError(int errorId, const QString &errorText) {
    m_errorId = errorId;
    m_errorText = errorText;
}

QVariant QnJsonRestResult::reply() const {
    return m_reply;
}

QByteArray QnJsonRestResult::serialize() {
    QJsonObject object;
    if(!m_errorText.isEmpty())
        object[QLatin1String("errorText")] = m_errorText;
    if(m_errorId != -1)
        object[QLatin1String("errorId")] = m_errorId;
    object[QLatin1String("reply")] = m_reply;

    QByteArray result;
    QJson::serialize(object, &result);
    return result;
}



// -------------------------------------------------------------------------- //
// QnJsonRestHandler
// -------------------------------------------------------------------------- //
QnJsonRestHandler::QnJsonRestHandler(): 
    m_contentType("application/json") 
{}

QnJsonRestHandler::~QnJsonRestHandler() {
    return;
}

int QnJsonRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    QnJsonRestResult jsonResult;
    int returnCode = executeGet(path, processParams(params), jsonResult);

    result = jsonResult.serialize();
    contentType = m_contentType;

    return returnCode;
}

int QnJsonRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    QnJsonRestResult jsonResult;
    int returnCode = executePost(path, processParams(params), body, jsonResult);

    result = jsonResult.serialize();
    contentType = m_contentType;

    return returnCode;
}

int QnJsonRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &, QnJsonRestResult &result) {
    return executeGet(path, params, result);
}

QnRequestParams QnJsonRestHandler::processParams(const QnRequestParamList &params) {
    QnRequestParams result;
    foreach(const QnRequestParam &param, params)
        result.insertMulti(param.first, param.second);
    return result;
}




