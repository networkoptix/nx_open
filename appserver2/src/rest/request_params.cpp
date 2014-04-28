/**********************************************************
* 30 jan 2014
* akolesnikov
***********************************************************/

#include "request_params.h"

#include <QtCore/QUrlQuery>

namespace ec2
{
    void parseHttpRequestParams(const QnRequestParamList &params, ApiStoredFilePath *path) {
        *path = params.value(lit("folder"));
    }

    void parseHttpRequestParams(const QnRequestParamList &params, QnId *id) {
        *id = params.value(lit("id"));
    }

    void parseHttpRequestParams(const QnRequestParamList &, nullptr_t *) {}
    void parseHttpRequestParams(const QnRequestParamList &, LoginInfo *) {}

    void toUrlParams(const QnId &id, QUrlQuery *query) {
        query->addQueryItem(lit("id"), id.toString());
    }

    void toUrlParams(const ApiStoredFilePath &name, QUrlQuery *query) {
        query->addQueryItem(lit("folder"), name);
    }

    void toUrlParams(const std::nullptr_t &, QUrlQuery *) {}
    void toUrlParams(const LoginInfo &, QUrlQuery *) {}

}

