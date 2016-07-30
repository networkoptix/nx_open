#ifndef NX_NETWORK_SOCKET_GLOBAL_H
#define NX_NETWORK_SOCKET_GLOBAL_H

#include <nx/utils/log/log.h>
#include <nx/utils/singleton.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/std/cpp14.h>

#include "aio/aioservice.h"

#include "cloud/address_resolver.h"
#include "cloud/cloud_connect_settings.h"
#include "cloud/mediator_address_publisher.h"
#include "cloud/mediator_connector.h"
#include "cloud/tunnel/outgoing_tunnel_pool.h"

#define NX_NETWORK_SOCKET_GLOBALS

namespace nx {
namespace network {

class NX_NETWORK_API SocketGlobals
{
public:
    inline static
    aio::AIOService& aioService()
    { return s_instance->m_aioService; }

    inline static
    cloud::AddressResolver& addressResolver()
    { return s_instance->m_addressResolver; }

    inline static
    cloud::MediatorAddressPublisher& addressPublisher()
    { return s_instance->m_addressPublisher; }

    inline static
    hpm::api::MediatorConnector& mediatorConnector()
    { return *s_instance->m_mediatorConnector; }

    inline static
    cloud::OutgoingTunnelPool& outgoingTunnelPool()
    { return s_instance->m_outgoingTunnelPool; }

    inline static
    cloud::CloudConnectSettings& cloudConnectSettings()
    { return s_instance->m_cloudConnectSettings; }

    static void init(); /**< Should be called before any socket use */
    static void deinit(); /**< Should be called when sockets are not needed any more */
    static void verifyInitialization();

    static void applyArguments(const utils::ArgumentParser& arguments);

    typedef void (*CustomInit)();
    typedef void (*CustomDeinit)();

    /** Invokes @param init only once, calls @param deinit in destructor */
    static void customInit(CustomInit init, CustomDeinit deinit = nullptr);

    class InitGuard
    {
    public:
        InitGuard() { init(); }
        ~InitGuard() { deinit(); }

        InitGuard( const InitGuard& ) = delete;
        InitGuard( InitGuard&& ) = delete;
        InitGuard& operator=( const InitGuard& ) = delete;
        InitGuard& operator=( InitGuard&& ) = delete;
    };

private:
    SocketGlobals();
    ~SocketGlobals();

    static QnMutex s_mutex;
    static std::atomic<bool> s_isInitialized;
    static size_t s_counter;
    static SocketGlobals* s_instance;

private:
    std::shared_ptr< QnLog::Logs > m_log;
    aio::AIOService m_aioService;

    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    cloud::AddressResolver m_addressResolver;
    cloud::MediatorAddressPublisher m_addressPublisher;
    cloud::OutgoingTunnelPool m_outgoingTunnelPool;
    cloud::CloudConnectSettings m_cloudConnectSettings;

    QnMutex m_mutex;
    std::map<CustomInit, CustomDeinit> m_customInits;
};

class SocketGlobalsHolder
:
    public Singleton<SocketGlobalsHolder>
{
public:
    SocketGlobalsHolder()
    :
        m_socketGlobalsGuard(std::make_unique<SocketGlobals::InitGuard>())
    {
    }

    void reinitialize()
    {
        m_socketGlobalsGuard.reset();
        m_socketGlobalsGuard = std::make_unique<SocketGlobals::InitGuard>();
    }

private:
    std::unique_ptr<SocketGlobals::InitGuard> m_socketGlobalsGuard;
};

} // namespace network
} // namespace nx

#endif  //NX_NETWORK_SOCKET_GLOBAL_H
