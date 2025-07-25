// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/std/expected.h>

namespace nx::network::upnp {

/**
 * UPnP SOAP based client.
 */
class NX_NETWORK_API AsyncClient
{
public:
    enum ErrorCode: int
    {
        unknown = -1,

        ok = 0,

        specifiedArrayIndexInvalid = 713,
        noSuchEntryInArray = 714,

        onlyPermanentLeasesSupported = 725,
    };

    enum class Protocol
    {
        tcp,
        udp
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(Protocol*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<Protocol>;
        return visitor(Item{Protocol::tcp, "TCP"}, Item{Protocol::udp, "UDP"});
    }

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
        quint16 internalPort = 0;
        quint16 externalPort = 0;
        Protocol protocol = Protocol::tcp;
        QString description;
        std::chrono::milliseconds duration;

        bool isValid() const;
        QString toString() const;
    };

    using MappingInfoResult = nx::utils::expected<MappingInfo, ErrorCode>;
    using MappingInfoCallback = std::function<void(MappingInfoResult)>;

    using MappingInfoList = std::vector<MappingInfo>;
    using MappingInfoListCallback = std::function<void(MappingInfoList, bool)>;

public:
    AsyncClient() = default;
    virtual ~AsyncClient();

    AsyncClient(const AsyncClient&) = delete;
    AsyncClient(AsyncClient&&) = delete;
    AsyncClient& operator=(const AsyncClient&) = delete;
    AsyncClient& operator=(AsyncClient&&) = delete;

    //! Creates request by @param message and calls @fn doPost
    void doUpnp(const nx::Url& url, const Message& message,
        std::function<void(const Message&)> callback);

    //! ID string of this client
    static constexpr auto kClientId = "NX UpnpAsyncClient";

    //! UPnP Device for @var kWanIp
    static constexpr auto kInternalGateway = "InternetGatewayDevice";

    //! UPnP Service for @fn externalIp, @fn addMapping
    static constexpr auto kWanIp = "WANIPConnection";

    //! Finds out external IP address
    virtual void externalIp(const nx::Url& url,
            std::function<void(const HostAddress&)> callback);

    //! Maps @param externalPort to @param internalPort on @param internalIp
    virtual void addMapping(const nx::Url& url, const HostAddress& internalIp,
        quint16 internalPort, quint16 externalPort,
        Protocol protocol, const QString& description, quint64 duration,
        std::function<void(ErrorCode)> callback);

    //! Removes mapping of @param externalPort
    virtual void deleteMapping(const nx::Url& url, quint16 externalPort, Protocol protocol,
        std::function<void(bool)> callback);

    //! Provides mapping info by @param index
    virtual void getMapping(const nx::Url& url, quint32 index,
        MappingInfoCallback callback);

    //! Provides mapping info by @param externalPort and @param protocol
    virtual void getMapping(const nx::Url& url, quint16 externalPort, Protocol protocol,
        MappingInfoCallback callback);

    void getAllMappings(const nx::Url& url, MappingInfoListCallback callback);

private:
    static ErrorCode getErrorCode(const Message& message);

    static void processMappingResponse(
        const Message& response,
        MappingInfoCallback callback,
        std::optional<Protocol> protocol = std::nullopt,
        std::optional<quint16> externalPort = std::nullopt);

private:
    nx::Mutex m_mutex;
    bool m_isTerminating = false;

    // TODO: replace with single httpClient when pipeline is supported
    std::set<nx::network::http::AsyncHttpClientPtr> m_httpClients;
};

} // namespace nx::network::upnp
