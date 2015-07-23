#ifndef UPNP_ASYNC_CLIENT_H
#define UPNP_ASYNC_CLIENT_H

#include "utils/common/model_functions_fwd.h"
#include "utils/network/http/asynchttpclient.h"

#include <set>

namespace nx_upnp {

//! UPnP SOAP based client
class AsyncClient
{   
public:
    enum Protocol { TCP, UDP };
    virtual ~AsyncClient() {}

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
                 std::function< void( const Message& ) > callback );

    //! ID string of this client
    static const QString CLIENT_ID;

    //! UPnP Device for @var WAN_IP
    static const QString INTERNAL_GATEWAY;

    //! Finds out external IP address
    virtual
    bool externalIp( const QUrl& url,
                     std::function< void( const HostAddress& ) > callback );

    //! UPnP Service for @fn externalIp, @fn addMapping
    static const QString WAN_IP;

    //! Maps @param externalPort to @param internalPort on @param internalIp
    virtual
    bool addMapping( const QUrl& url, const HostAddress& internalIp,
                     quint16 internalPort, quint16 externalPort,
                     Protocol protocol, const QString& description,
                     std::function< void( bool ) > callback );

    //! Removes mapping of @param externalPort
    virtual
    bool deleteMapping( const QUrl& url, quint16 externalPort, Protocol protocol,
                        std::function< void( bool ) > callback );

    struct MappingInfo
    {
        HostAddress internalIp;
        quint16     internalPort;
        quint16     externalPort;
        Protocol    protocol;
        QString     description;

        MappingInfo( const HostAddress& inIp = HostAddress(),
                     quint16 inPort = 0, quint16 exPort = 0, Protocol prot = TCP,
                     const QString& desc = QString() );
        bool isValid() const;
    };

    //! Provides mapping info by @param index
    virtual
    bool getMapping( const QUrl& url, quint32 index,
                     std::function< void( MappingInfo ) > callback );

    //! Provides mapping info by @param externalPort and @param protocol
    virtual
    bool getMapping( const QUrl& url, quint16 externalPort, Protocol protocol,
                     std::function< void( MappingInfo ) > callback );

    typedef std::vector< MappingInfo > MappingList;

    bool getAllMappings( const QUrl& url,
                         std::function< void( MappingList ) > callback  );

private:
    // TODO: replace with single httpClient when pipeline is supported
    QMutex m_mutex;
    std::set< nx_http::AsyncHttpClientPtr > m_httpClients;
};

QN_FUSION_DECLARE_FUNCTIONS( AsyncClient::Protocol, ( lexical ) )

} // namespace nx_upnp

#endif // UPNP_ASYNC_CLIENT_H
