/**********************************************************
* 30 jan 2014
* akolesnikov
***********************************************************/

#include "request_params.h"

#include <QtCore/QUrlQuery>

namespace ec2
{
    void parseHttpRequestParams(const QnRequestParamList &params, ApiStoredFilePath *path)
    {
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i].first == lit("folder"))
                *path = params[i].second;
        }
    }

    void parseHttpRequestParams(const QnRequestParamList &params, QnId *id)
    {
        foreach(const QnRequestParamList::value_type &val, params)
        {
            if(val.first == lit("id"))
            {
                *id = val.second;
                return;
            }
        }
    }

    void parseHttpRequestParams(const QnRequestParamList &, nullptr_t *) {}
    void parseHttpRequestParams(const QnRequestParamList &, LoginInfo *) {}

    void toUrlParams(const QnId &id, QUrlQuery *query)
    {
        query->addQueryItem(lit("id"), id.toString());
    }

    void toUrlParams(const ApiStoredFilePath &name, QUrlQuery *query)
    {
        query->addQueryItem(lit("folder"), name);
    }

    void toUrlParams(const std::nullptr_t &, QUrlQuery *) {}
    void toUrlParams(const LoginInfo &, QUrlQuery *) {}

}

