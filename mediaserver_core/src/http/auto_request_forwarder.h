#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/http_types.h>
#include <common/common_module_aware.h>


//!Forwards requests addressed to resources belonging to other servers
class QnAutoRequestForwarder: public QnCommonModuleAware
{
public:
    QnAutoRequestForwarder(QnCommonModule* commonModule);

    //!Analyzes request and adds proxy information if needed
    void processRequest( nx_http::Request* const request );

    void addPathToIgnore(const QString& pathWildcardMask);
private:
    nx_http::AuthMethodRestrictionList m_restrictionList;

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
