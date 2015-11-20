/**********************************************************
* Jul 22, 2015
* a.kolesnikov
***********************************************************/

#ifndef HTTP_MOD_MANAGER_H
#define HTTP_MOD_MANAGER_H

#include <functional>
#include <list>
#include <map>

#include <utils/common/singleton.h>

#include "httptypes.h"


namespace nx_http
{
    //!This class is to manage all modifications to HTTP request/response
    class HttpModManager
    :
        public Singleton<HttpModManager>
    {
    public:
        //!Performs some modifications on \a request
        void apply( Request* const request );

        //!Replaces path \a originalPath to \a effectivePath
        void addUrlRewriteExact( const QString& originalPath, const QString& effectivePath );
        //!Register functor that will receive every incoming request before any processing and have a chance to modify it
        void addCustomRequestMod( std::function<void(Request*)> requestMod );

    private:
        std::map<QString, QString> m_urlRewriteExact;
        std::list<std::function<void( Request* )>> m_requestModifiers;
    };
}

#endif  //HTTP_MOD_MANAGER_H
