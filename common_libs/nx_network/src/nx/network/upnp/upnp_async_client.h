#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/deprecated/asynchttpclient.h>

#include <set>

namespace nx_upnp {

/**
 * UPnP SOAP based client.
 */
class NX_NETWORK_API AsyncClient
{
public:
    enum Protocol { TCP, UDP };

    AsyncClient() = default;
    virtual ~AsyncClient();

    AsyncClient(const AsyncClient&) = delete;
    AsyncClient(AsyncClient&&) = delete;
    AsyncClient& operator=(const AsyncClient&) = delete;
    AsyncClient& operator=(AsyncClient&&) = delete;

    //! Simple SOAP call
    struct NX_NETWORK_API Message
    {
        QString action;
        QString service;
        std::map<QString, QString> params;

        //! @returns wether message represents normal resquest/response or error
        //! NOTE: in case of error @var action and @var service are empty
        bool isOk() const;

        //! @returns value or empty string
        const QString& getParam(const QString& key) const;

        QString toString() const;
    };

    //! Creates request by @param message and calls @fn doPost
    void doUpnp(const nx::utils::Url& url, const Message& message,
        std::function< void(const Message&) > callback);

    //! ID string of this client
    static const QString CLIENT_ID;

    //! UPnP Device for @var WAN_IP
    static const QString INTERNAL_GATEWAY;

    //! Finds out external IP address
    virtual
        void externalIp(const nx::utils::Url& url,
            std::function< void(const HostAddress&) > callback);

    //! UPnP Service for @fn externalIp, @fn addMapping
    static const QString WAN_IP;

    //! Maps @param externalPort to @param internalPort on @param internalIp
    virtual
        void addMapping(const nx::utils::Url& url, const HostAddress& internalIp,
            quint16 internalPort, quint16 externalPort,
            Protocol protocol, const QString& description, quint64 duration,
            std::function< void(bool) > callback);

    //! Removes mapping of @param externalPort
    virtual
        void deleteMapping(const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
            std::function< void(bool) > callback);

    struct NX_NETWORK_API MappingInfo
    {
        HostAddress internalIp;
        quint16     internalPort;
        quint16     externalPort;
        Protocol    protocol;
        QString     description;
        quint64     duration;

        MappingInfo(const HostAddress& inIp = HostAddress(),
            quint16 inPort = 0, quint16 exPort = 0,
            Protocol prot = Protocol::TCP,
            const QString& desc = QString(), quint64 dur = 0);

        bool isValid() const;
        QString toString() const;
    };

    //! Provides mapping info by @param index
    virtual
        void getMapping(const nx::utils::Url& url, quint32 index,
            std::function< void(MappingInfo) > callback);

    //! Provides mapping info by @param externalPort and @param protocol
    virtual
        void getMapping(const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
            std::function< void(MappingInfo) > callback);

    typedef std::vector< MappingInfo > MappingList;

    void getAllMappings(const nx::utils::Url& url,
        std::function< void(MappingList) > callback);

private:
    // TODO: replace with single httpClient when pipeline is supported
    QnMutex m_mutex;
    std::set< nx_http::AsyncHttpClientPtr > m_httpClients;
};

} // namespace nx_upnp

QN_FUSION_DECLARE_FUNCTIONS(nx_upnp::AsyncClient::Protocol, (lexical));
