/**********************************************************
* 30 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_REST_REQUEST_PARAMS_H
#define EC2_REST_REQUEST_PARAMS_H

#include <utils/common/id.h>
#include <utils/common/request_param.h>

#include <nx_ec/data/api_fwd.h>

class QUrlQuery;

namespace ec2
{
    void parseHttpRequestParams(const QnRequestParamList &params, ApiStoredFilePath *path);
    void parseHttpRequestParams(const QnRequestParamList &params, LoginInfo *loginInfo);
    void parseHttpRequestParams(const QnRequestParamList &params, QnId *id);
    void parseHttpRequestParams(const QnRequestParamList &params, nullptr_t *);
    void toUrlParams(const std::nullptr_t &, QUrlQuery *query);
    void toUrlParams(const QnId & id, QUrlQuery *query);
    void toUrlParams(const ApiStoredFilePath &id, QUrlQuery *query);
    void toUrlParams(const LoginInfo &loginInfo, QUrlQuery *query);
}

#endif  //EC2_REST_REQUEST_PARAMS_H
