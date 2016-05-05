/**********************************************************
* Aug 27, 2015
* a.kolesnikov
***********************************************************/

#include "auth_restriction_list.h"

#include "utils/match/wildcard.h"
#include <network/tcp_listener.h>


QnAuthMethodRestrictionList::QnAuthMethodRestrictionList()
{
}

unsigned int QnAuthMethodRestrictionList::getAllowedAuthMethods( const nx_http::Request& request ) const
{
    QString path = QnTcpListener::normalizedPath(request.requestLine.url.path());
    unsigned int allowed = AuthMethod::cookie | AuthMethod::http | AuthMethod::videowall | AuthMethod::urlQueryParam;   //by default
    for( std::pair<QString, unsigned int> allowRule : m_allowed )
    {
        if( !wildcardMatch( allowRule.first, path ) )
            continue;
        allowed |= allowRule.second;
    }

    for( std::pair<QString, unsigned int> denyRule : m_denied )
    {
        if( !wildcardMatch( denyRule.first, path ) )
            continue;
        allowed &= ~denyRule.second;
    }

    return allowed;
}

void QnAuthMethodRestrictionList::allow( const QString& pathMask, AuthMethod::Value method )
{
    m_allowed[pathMask] = method;
}

void QnAuthMethodRestrictionList::deny( const QString& pathMask, AuthMethod::Value method )
{
    m_denied[pathMask] = method;
}

