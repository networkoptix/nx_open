/**********************************************************
* 30 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_REST_REQUEST_PARAMS_H
#define EC2_REST_REQUEST_PARAMS_H

#include <algorithm>

#include <QtCore/QUrlQuery>

#include <core/resource/resource.h>
#include <nx_ec/data/connection_data.h>
#include <utils/common/id.h>
#include <utils/common/request_param.h>


namespace ec2
{
    void parseHttpRequestParams( const QnRequestParamList& params, QnId* id);
    void parseHttpRequestParams( const QnRequestParamList& params, nullptr_t* );
    void toUrlParams( const std::nullptr_t& , QUrlQuery* const query );
    void toUrlParams( const QnId& id, QUrlQuery* const query );
    void toUrlParams( const LoginInfo& loginInfo, QUrlQuery* const query );
}

#endif  //EC2_REST_REQUEST_PARAMS_H
