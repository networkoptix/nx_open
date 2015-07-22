/**********************************************************
* Jul 22, 2015
* a.kolesnikov
***********************************************************/

#include "http_mod_manager.h"


namespace nx_http
{
    void HttpModManager::apply( Request* const request )
    {
        //applying m_urlRewriteExact
        auto it = m_urlRewriteExact.find( request->requestLine.url.path() );
        if( it != m_urlRewriteExact.end() )
            request->requestLine.url.setPath( it->second );
    }

    void HttpModManager::addUrlRewriteExact( const QString& originalPath, const QString& effectivePath )
    {
        m_urlRewriteExact.emplace( originalPath, effectivePath );
    }
}
