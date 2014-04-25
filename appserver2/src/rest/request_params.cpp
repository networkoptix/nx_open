/**********************************************************
* 30 jan 2014
* akolesnikov
***********************************************************/

#include "request_params.h"

#include <QtCore/QUrlQuery>

namespace ec2
{
    static const QString ID_PARAM_NAME( lit("id") );
    static const QString FOLDER_NAME_PARAM_NAME( lit("folder") );

    void parseHttpRequestParams( const QnRequestParamList& params, ApiStoredFilePath* path )
    {
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i].first == lit("folder"))
                *path = params[i].second;
        }
    }

    void parseHttpRequestParams( const QnRequestParamList& params, QnId* id)
    {
        foreach( const QnRequestParamList::value_type& val, params )
        {
            if( val.first == ID_PARAM_NAME )
            {
                *id = val.second;
                return;
            }
        }
    }

    void parseHttpRequestParams( const QnRequestParamList& /*params*/, nullptr_t* ) {}

    void toUrlParams( const std::nullptr_t& , QUrlQuery* const /*query*/)
    {
        // nothing to do
    }

    void toUrlParams( const QnId& id, QUrlQuery* const query)
    {
        query->addQueryItem( ID_PARAM_NAME, id.toString() );
    }

    void toUrlParams( const ApiStoredFilePath& name, QUrlQuery* const query)
    {
        query->addQueryItem( FOLDER_NAME_PARAM_NAME, name );
    }

    void toUrlParams( const LoginInfo& /*loginInfo*/, QUrlQuery* const /*query*/)
    {
        //TODO/IMPL
    }
}
