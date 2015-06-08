#ifndef UPNP_ASYNC_CLIENT_H
#define UPNP_ASYNC_CLIENT_H

#include "utils/network/http/asynchttpclient.h"

#include <set>

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

        //! @returns wether message represents normal resquest/response or error
        //! @note in case of error @var action and @var service are empty
        bool isOk() const;

        //! @returns value or empty string
        const QString& getParam( const QString& key ) const;
    };

    //! Creates request by @param message and calls @fn doPost
    bool doUpnp( const QUrl& url, const Message& message,
                 const std::function< void( const Message& ) >& callback );

    //! ID string of this client
    static const QString CLIENT_ID;

    //! UPnP Device for @var WAN_IP
    static const QString INTERNAL_GATEWAY;

    //! Finds out external IP address
    bool externalIp( const QUrl& url,
                     const std::function< void( const HostAddress& ) >& callback );

    //! UPnP Service for @fn externalIp, @fn addMapping
    static const QString WAN_IP;

    //! Maps @param externalPort to @param internalPort on @param internalIp
    bool addMapping( const QUrl& url, const HostAddress& internalIp,
                     quint16 internalPort, quint16 externalPort, const QString& protocol,
                     const std::function< void( bool ) >& callback );

    //! Removes mapping of @param externalPort
    bool deleteMapping( const QUrl& url, quint16 externalPort, const QString& protocol,
                        const std::function< void( bool ) >& callback );

    typedef std::function< void(
                const HostAddress& /*internalIp*/,
                quint16 /*internalPort*/, quint16 /*externalPort*/, const QString& /*protocol*/
            )> MappingInfoCallback;

    //! Provides mapping info by @param index
    bool getMapping( const QUrl& url, quint32 index,
                     const MappingInfoCallback& callback );

    //! Provides mapping info by @param externalPort and @param protocol
    bool getMapping( const QUrl& url, quint16 externalPort, const QString& protocol,
                     const MappingInfoCallback& callback );

private:
    // TODO: replace with single httpClient when pipeline is supported
    QMutex m_mutex;
    std::set< nx_http::AsyncHttpClientPtr > m_httpClients;
};

#endif // UPNP_ASYNC_CLIENT_H
