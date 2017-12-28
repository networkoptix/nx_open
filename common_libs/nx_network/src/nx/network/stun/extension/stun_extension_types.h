#pragma once

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/socket_common.h>
#include <nx/network/stun/message.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace network {
namespace stun {
namespace extension {

namespace methods {

enum Value
{
    /**
     * Pings mediaserver endpoints and reports verified onese.
     * Request: SystemId, ServerId, PublicEndpointList.
     * Response: PublicEndpointList (vrified).
     */
    ping = stun::MethodType::userMethod,

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

    /** Notifies mediator about connection attemt result. */
    connectionResult,

    udpHolePunchingSyn,
    tunnelConnectionChosen,

    /** Registers client for incoming connections (for reverse connect). */
    clientBind,

    /** Veifies current peer state from mediator's perspective (e.g. is listening). */
    getConnectionState,
};

NX_NETWORK_API nx::String toString(Value val);

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

    systemErrorCode = stun::attrs::userDefined + 0x500,
};

NX_NETWORK_API const char* toString(AttributeType val);


/** Base class for string attributes */
struct NX_NETWORK_API BaseStringAttribute : stun::attrs::Unknown
{
    BaseStringAttribute( int userType, const String& value = String() );
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
        StringAttribute( const String& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API SystemId : BaseStringAttribute
{
    static const AttributeType TYPE = systemId;
    SystemId( const String& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API ServerId : BaseStringAttribute
{
    static const AttributeType TYPE = serverId;
    ServerId( const String& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API PeerId : BaseStringAttribute
{
    static const AttributeType TYPE = peerId;
    PeerId( const String& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API ConnectionId : BaseStringAttribute
{
    static const AttributeType TYPE = connectionId;
    ConnectionId( const String& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API HostName : BaseStringAttribute
{
    static const AttributeType TYPE = hostName;
    HostName( const String& value ) : BaseStringAttribute( TYPE, value ) {}
};

struct NX_NETWORK_API ConnectionMethods: BaseStringAttribute
{
    static const AttributeType TYPE = connectionMethods;
    ConnectionMethods(const String& value): BaseStringAttribute(TYPE, value) {}
};

struct NX_NETWORK_API UdpHolePunchingResultCodeAttr: stun::attrs::IntAttribute
{
    static const AttributeType TYPE = udpHolePunchingResultCode;
    UdpHolePunchingResultCodeAttr(int value)
        : stun::attrs::IntAttribute(TYPE, value) {}
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
    EndpointList( int type, const std::list< SocketAddress >& endpoints );
    std::list< SocketAddress > get() const;
};

struct NX_NETWORK_API PublicEndpointList : EndpointList
{
    static const AttributeType TYPE = publicEndpointList;
    PublicEndpointList( const std::list< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

struct NX_NETWORK_API TcpHpEndpointList : EndpointList
{
    static const AttributeType TYPE = tcpHpEndpointList;
    TcpHpEndpointList( const std::list< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

struct NX_NETWORK_API UdtHpEndpointList : EndpointList
{
    static const AttributeType TYPE = udtHpEndpointList;
    UdtHpEndpointList( const std::list< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

struct NX_NETWORK_API TrafficRelayUrl: BaseStringAttribute
{
    static const AttributeType TYPE = trafficRelayUrl;
    TrafficRelayUrl(const String& value)
        : BaseStringAttribute( TYPE, value) {}
};

struct NX_NETWORK_API TcpReverseEndpointList : EndpointList
{
    static const AttributeType TYPE = tcpReverseEndpointList;
    TcpReverseEndpointList( const std::list< SocketAddress >& endpoints )
        : EndpointList( TYPE, endpoints ) {}
};

/** Base class for string list based attributes */
struct NX_NETWORK_API StringList : BaseStringAttribute
{
    StringList( int type, const std::vector< String >& strings );
    std::vector< String > get() const;
};

struct NX_NETWORK_API HostNameList : StringList
{
    static const AttributeType TYPE = hostNameList;
    HostNameList( const std::vector< String >& hostNames )
        : StringList( TYPE, hostNames ) {}
};

} // namespace attrs

} // namespace extension
} // namespace stun
} // namespace network
} // namespace nx
