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
    bool parseHttpRequestParams(const QnRequestParamList &params, ApiStoredFilePath *path);
    void toUrlParams(const ApiStoredFilePath &id, QUrlQuery *query);

    bool parseHttpRequestParams(const QnRequestParamList &params, QnId *id);
    void toUrlParams(const QnId &id, QUrlQuery *query);

    bool parseHttpRequestParams(const QnRequestParamList &params, Qn::SerializationFormat *format);
    void toUrlParams(const Qn::SerializationFormat& format, QUrlQuery *query);

    bool parseHttpRequestParams(const QnRequestParamList &params, ApiLoginData *loginInfo);
    void toUrlParams(const ApiLoginData &, QUrlQuery *query);

    bool parseHttpRequestParams(const QnRequestParamList &params, nullptr_t *);
    void toUrlParams(const std::nullptr_t &, QUrlQuery *query);
}

#endif  //EC2_REST_REQUEST_PARAMS_H
