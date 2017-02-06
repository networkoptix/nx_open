/**********************************************************
* Jul 28, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_AUTO_REQUEST_FORWARDER_H
#define NX_AUTO_REQUEST_FORWARDER_H

#include <core/resource/resource_fwd.h>
#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/httptypes.h>


//!Forwards requests addressed to resources belonging to other servers
class QnAutoRequestForwarder
{
public:
    //!Analyzes request and adds proxy information if needed
    void processRequest( nx_http::Request* const request );

    void addPathToIgnore(const QString& pathWildcardMask);

private:
    QnAuthMethodRestrictionList m_restrictionList;

    bool findCameraGuid(
        const nx_http::Request& request,
        const QUrlQuery& urlQuery,
        QnResourcePtr* const res );
    bool findCameraUniqueID(
        const nx_http::Request& request,
        const QUrlQuery& urlQuery,
        QnResourcePtr* const res );
    bool findCameraUniqueIDInPath(
        const nx_http::Request& request,
        QnResourcePtr* const res );
    bool findCameraUniqueIDInQuery(
        const QUrlQuery& urlQuery,
        QnResourcePtr* const res );
    /*!
        \return UTC time (milliseconds)
    */
    qint64 fetchTimestamp(
        const nx_http::Request& request,
        const QUrlQuery& urlQuery );

    /*
     * Update request to proxy it to the specified server
     * return true if request forwarded
     */
    bool addProxyToRequest(
        nx_http::Request* const request,
        const QnMediaServerResourcePtr& serverRes);
};

#endif  //NX_AUTO_REQUEST_FORWARDER_H
