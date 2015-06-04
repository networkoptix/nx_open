#ifndef UPNP_ASYNC_CLIENT_H
#define UPNP_ASYNC_CLIENT_H

#include "utils/network/http/asynchttpclient.h"

#include <set>

class UpnpAsyncClient;
typedef std::shared_ptr<UpnpAsyncClient> UpnpAsyncClientPtr;

//! UPnP SOAP based client
class UpnpAsyncClient
{   
public:
    //! Simple SOAP call
    struct Message
    {
        QString action;
        QString service;
        std::map<QString, QString> params;

        //! @returns value or empty string
        const QString& getParam( const QString& key ) const;
    };

    //!Creates request by @param message and calls @fn doPost
    bool doUpnp( const QUrl& url, const Message& message,
                 const std::function< void( const Message& ) >& callback );

    bool externalIp( const QUrl& url,
                     const std::function< void( const QString& ) >& callback );

    /*
    struct Mapping
    {
        QString     internalIp;
        quint16_t   internalPort;
        quint16_t   externalPort;
        QString     protocol;
    };

    bool addMapping( const QUrl& url, const Mapping& mapping,
                     const std::function< Mapping >& callback );
    */

private:
    // TODO: replace with single httpClient when pipeline is supported
    std::set< nx_http::AsyncHttpClientPtr > m_httpClients;
};

#endif // UPNP_ASYNC_CLIENT_H
