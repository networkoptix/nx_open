// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>
#include <vector>

#include <nx/reflect/type_utils.h>

#include <nx/network/cloud/cloud_connect_type.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>
#include <nx/utils/uuid.h>

namespace nx::network::stun::extension {

namespace methods {

enum Value
{
    reserved1 = stun::MethodType::userMethod,

    /**
     * Registers mediaserver for external connections.
     * Request: SystemId, ServerId, PublicEndpointList.
     */
    bind,

    /** Notifies mediator that server is ready to accept cloud connections. */
    listen,

    /** Server uses this request to confirm its willingness to proceed with cloud connection. */
    connectionAck,

    /** Returns host name list included in domain. */
    resolveDomain,

    /** Returns host's public address list and suitable connections methods. */
    resolvePeer,

    /**
     * Initiate connection to some mediaserver.
     * Request: PeerId, HostName, ConnectionId.
     * Response: PublicEndpointList (opt), TcpHpEndpointList (opt), UdtHpEndpointList (opt).
     */
    connect,

    /** Notifies mediator about connection attempt result. */
    connectionResult,

    udpHolePunchingSyn,
    tunnelConnectionChosen,

    /**
     * Used to be "clientBind".
     * NOTE: Cannot be dropped to keep numeric values of methods unchanged.
     */
    reserved,

    /** Verifies current peer state from mediator's perspective (e.g. is listening). */
    getConnectionState,
};

NX_NETWORK_API std::string_view toString(Value val);

} // namespace methods

namespace indications {

enum Value
{
    /**
     * Indicates requested connection.
     * Attrs: PeerId, ConnectionId, PublicEndpointList (opt), TcpHpEndpointList (opt),
     *     UdtHpEndpointList (opt)
     */
    connectionRequested = MethodType::userIndication,

    /**
     * Indicates update information about on-going connection establishment
     * Attrs: ConnectionId, PublicEndpointList (opt), TcpHpEndpointList (opt),
     *     UdtHpEndpointList (opt)
     */
    connectionUpdate
};

} // namespace indications

namespace error {

enum Value
{
    notFound = 404,
};

} // namespace error

//TODO custom stun requests parameters
namespace attrs {

enum AttributeType
{
    resultCode = stun::attrs::userDefined,

    systemId,
    serverId,
    peerId,
    connectionId,
    cloudConnectVersion,
    cloudConnectOptions,

    hostName = stun::attrs::userDefined + 0x200,
    hostNameList,
    publicEndpointList,
    tcpHpEndpointList,
    udtHpEndpointList,
    connectionMethods,
    ignoreSourceAddress,
    tcpReverseEndpointList,
    isPersistent,
    isListening,
    trafficRelayUrl,
    trafficRelayUrlList,
    trafficRelayConnectTimeout,

    udpHolePunchingResultCode = stun::attrs::userDefined + 0x400,
    rendezvousConnectTimeout,
    udpTunnelKeepAliveInterval,
    udpTunnelKeepAliveRetries,
    tcpReverseRetryMaxCount,
    tcpReverseRetryInitialDelay,
    tcpReverseRetryDelayMultiplier,
    tcpReverseRetryMaxDelay,
    tcpReverseHttpSendTimeout,
    tcpReverseHttpReadTimeout,
    tcpReverseHttpMsgBodyTimeout,
    tunnelInactivityTimeout,
    tcpConnectionKeepAlive,
    udpHolePunchingStartDelay,
    trafficRelayingStartDelay,
    directTcpConnectStartDelay,
    connectType,
    httpStatusCode,
    duration,
    tunnelConnectResponseTime,
    mediatorResponseTime,


    systemErrorCode = stun::attrs::userDefined + 0x500,
};

NX_NETWORK_API const char* toString(AttributeType val);

/** Base class for string attributes */
struct NX_NETWORK_API BaseStringAttribute : stun::attrs::Unknown
{
    BaseStringAttribute( int userType, const std::string& value = std::string() );
};

struct NX_NETWORK_API ResultCode: stun::attrs::IntAttribute
{
    static const AttributeType TYPE = resultCode;

    ResultCode(const nx::hpm::api::ResultCode& value)
        : stun::attrs::IntAttribute(TYPE, static_cast<int>(value)) {}

    nx::hpm::api::ResultCode value() const
    {
        return static_cast<nx::hpm::api::ResultCode>(
            stun::attrs::IntAttribute::value());
    }
};

template<AttributeType attributeType>
struct StringAttribute: BaseStringAttribute
{
        static const AttributeType TYPE = attributeType;
        StringAttribute( const std::string& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API SystemId : BaseStringAttribute
{
    static const AttributeType TYPE = systemId;
    SystemId( const std::string& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API ServerId : BaseStringAttribute
{
    static const AttributeType TYPE = serverId;
    ServerId( const std::string& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API PeerId : BaseStringAttribute
{
    static const AttributeType TYPE = peerId;
    PeerId( const std::string& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API ConnectionId : BaseStringAttribute
{
    static const AttributeType TYPE = connectionId;
    ConnectionId( const std::string& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API HostName : BaseStringAttribute
{
    static const AttributeType TYPE = hostName;
    HostName( const std::string& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API ConnectionMethods: BaseStringAttribute
{
    static const AttributeType TYPE = connectionMethods;
    ConnectionMethods(const std::string& value): BaseStringAttribute(TYPE, value) {}
};

struct NX_NETWORK_API UdpHolePunchingResultCodeAttr: stun::attrs::IntAttribute
{
    static const AttributeType TYPE = udpHolePunchingResultCode;
    UdpHolePunchingResultCodeAttr(int value)
        : stun::attrs::IntAttribute(TYPE, value) {}
};

struct NX_NETWORK_API ConnectTypeAttr: stun::attrs::IntAttribute
{
    static const AttributeType TYPE = connectType;
    ConnectTypeAttr(cloud::ConnectType value)
        : stun::attrs::IntAttribute(TYPE, (int) value) {}
};

struct NX_NETWORK_API SystemErrorCodeAttr: stun::attrs::IntAttribute
{
    static const AttributeType TYPE = systemErrorCode;
    SystemErrorCodeAttr(int value)
        : stun::attrs::IntAttribute(TYPE, value) {}
};

struct NX_NETWORK_API Endpoint: BaseStringAttribute
{
    Endpoint(int type, const SocketAddress& endpoint);
    SocketAddress get() const;
};

/** Base class for endpoint attributes. */
struct NX_NETWORK_API EndpointList : BaseStringAttribute
{
    EndpointList( int type, const std::vector< SocketAddress >& endpoints );
    std::vector< SocketAddress > get() const;
};

struct NX_NETWORK_API PublicEndpointList : EndpointList
{
    static const AttributeType TYPE = publicEndpointList;
    PublicEndpointList( const std::vector< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

struct NX_NETWORK_API TcpHpEndpointList : EndpointList
{
    static const AttributeType TYPE = tcpHpEndpointList;
    TcpHpEndpointList( const std::vector< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

struct NX_NETWORK_API UdtHpEndpointList : EndpointList
{
    static const AttributeType TYPE = udtHpEndpointList;
    UdtHpEndpointList( const std::vector< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

struct NX_NETWORK_API TrafficRelayUrl: BaseStringAttribute
{
    static const AttributeType TYPE = trafficRelayUrl;
    TrafficRelayUrl(const std::string& value)
        : BaseStringAttribute( TYPE, value) {}
};

struct NX_NETWORK_API TcpReverseEndpointList : EndpointList
{
    static const AttributeType TYPE = tcpReverseEndpointList;
    TcpReverseEndpointList( const std::vector< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

/** Base class for string list based attributes */
struct NX_NETWORK_API StringList : BaseStringAttribute
{
    StringList( int type, const std::vector< std::string >& strings );
    std::vector< std::string > get() const;
};

struct NX_NETWORK_API HostNameList : StringList
{
    static const AttributeType TYPE = hostNameList;
    HostNameList( const std::vector< std::string >& hostNames )
        : StringList( TYPE, hostNames ) {}
};

struct NX_NETWORK_API HttpStatusCode : stun::attrs::IntAttribute
{
    static constexpr AttributeType TYPE = httpStatusCode;
    HttpStatusCode(int value) :
        stun::attrs::IntAttribute(TYPE, value) {}

    inline int get() const { return value(); }
};

struct NX_NETWORK_API Duration : stun::attrs::Int64Attribute
{
    static constexpr AttributeType TYPE = duration;

    template<
        typename StdChronoDuration,
        typename = std::enable_if_t<nx::reflect::IsStdChronoDurationV<StdChronoDuration>>
    >
    Duration(StdChronoDuration value) :
        stun::attrs::Int64Attribute(TYPE, value.count()) {}
};

struct NX_NETWORK_API TunnelConnectResponseTime : stun::attrs::Int64Attribute
{
    static constexpr AttributeType TYPE = tunnelConnectResponseTime;

    template<
        typename StdChronoDuration,
        typename = std::enable_if_t<nx::reflect::IsStdChronoDurationV<StdChronoDuration>>
    >
    TunnelConnectResponseTime(StdChronoDuration value) :
        stun::attrs::Int64Attribute(TYPE, value.count()) {}
};

struct NX_NETWORK_API MediatorResponseTime : stun::attrs::Int64Attribute
{
    static constexpr AttributeType TYPE = mediatorResponseTime;

    template<
        typename StdChronoDuration,
        typename = std::enable_if_t<nx::reflect::IsStdChronoDurationV<StdChronoDuration>>
    >
    MediatorResponseTime(StdChronoDuration value) :
        stun::attrs::Int64Attribute(TYPE, value.count()) {}
};

} // namespace attrs

} // namespace nx::network::stun::extension
