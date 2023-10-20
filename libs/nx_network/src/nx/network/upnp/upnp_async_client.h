// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/reflect/enum_instrument.h>

namespace nx::network::upnp {

/**
 * UPnP SOAP based client.
 */
class NX_NETWORK_API AsyncClient
{
public:
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Protocol,
        tcp,
        udp
    )

    //! Simple SOAP call
    struct NX_NETWORK_API Message
    {
        Message() = default;
        Message(QString action, QString service):
            action(std::move(action)), service(std::move(service)) {}

        QString action;
        QString service;
        std::map<QString, QString> params;

        //! @returns whether message represents normal request/response or error
        //! NOTE: in case of error @var action and @var service are empty
        bool isOk() const;

        //! @returns value or empty string
        const QString& getParam(const QString& key) const;

        QString toString() const;
    };

    struct NX_NETWORK_API MappingInfo
    {
        HostAddress internalIp;
        quint16 internalPort;
        quint16 externalPort;
        Protocol protocol;
        QString description;
        std::chrono::milliseconds duration;

        MappingInfo(const HostAddress& inIp = HostAddress(),
            quint16 inPort = 0,
            quint16 exPort = 0,
            Protocol prot = Protocol::tcp,
            const QString& desc = QString(),
            quint64 dur = 0);

        bool isValid() const;
        QString toString() const;
    };

    using MappingList = std::vector<MappingInfo>;

public:
    AsyncClient() = default;
    virtual ~AsyncClient();

    AsyncClient(const AsyncClient&) = delete;
    AsyncClient(AsyncClient&&) = delete;
    AsyncClient& operator=(const AsyncClient&) = delete;
    AsyncClient& operator=(AsyncClient&&) = delete;

    //! Creates request by @param message and calls @fn doPost
    void doUpnp(const nx::utils::Url& url, const Message& message,
        std::function<void(const Message&)> callback);

    //! ID string of this client
    static constexpr auto kClientId = "NX UpnpAsyncClient";

    //! UPnP Device for @var kWanIp
    static constexpr auto kInternalGateway = "InternetGatewayDevice";

    //! UPnP Service for @fn externalIp, @fn addMapping
    static constexpr auto kWanIp = "WANIPConnection";

    //! Finds out external IP address
    virtual void externalIp(const nx::utils::Url& url,
            std::function<void(const HostAddress&)> callback);

    //! Maps @param externalPort to @param internalPort on @param internalIp
    virtual void addMapping(const nx::utils::Url& url, const HostAddress& internalIp,
            quint16 internalPort, quint16 externalPort,
            Protocol protocol, const QString& description, quint64 duration,
            std::function<void(bool)> callback);

    //! Removes mapping of @param externalPort
    virtual void deleteMapping(const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
            std::function<void(bool)> callback);

    //! Provides mapping info by @param index
    virtual void getMapping(const nx::utils::Url& url, quint32 index,
            std::function<void(MappingInfo)> callback);

    //! Provides mapping info by @param externalPort and @param protocol
    virtual void getMapping(const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
            std::function<void(MappingInfo)> callback);

    void getAllMappings(const nx::utils::Url& url,
        std::function<void(MappingList)> callback);

private:
    nx::Mutex m_mutex;
    bool m_isTerminating = false;

    // TODO: replace with single httpClient when pipeline is supported
    std::set<nx::network::http::AsyncHttpClientPtr> m_httpClients;
};

} // namespace nx::network::upnp
