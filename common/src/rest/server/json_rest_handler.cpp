#include "json_rest_handler.h"

QnJsonRestHandler::QnJsonRestHandler(): 
    m_contentType("application/json") 
{}

QnJsonRestHandler::~QnJsonRestHandler() {
    return;
}

int QnJsonRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    QnJsonRestResult jsonResult;
    int returnCode = executeGet(path, processParams(params), jsonResult);

    result = QJson::serialized(jsonResult);
    contentType = m_contentType;

    return returnCode;
}

int QnJsonRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, const QByteArray& /*srcBodyContentType*/, QByteArray &result, QByteArray &contentType)
{
    QnJsonRestResult jsonResult;
    int returnCode = executePost(path, processParams(params), body, jsonResult);

    result = QJson::serialized(jsonResult);
    contentType = m_contentType;

    return returnCode;
}

int QnJsonRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &, QnJsonRestResult &result) {
    return executeGet(path, params, result);
}

int QnJsonRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    return executePost(path, params, QByteArray(), result);
}

QnRequestParams QnJsonRestHandler::processParams(const QnRequestParamList &params) const {
    QnRequestParams result;
    foreach(const QnRequestParam &param, params)
        result.insertMulti(param.first, param.second);
    return result;
}




