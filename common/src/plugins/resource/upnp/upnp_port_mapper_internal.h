#ifndef UPNP_PORT_MAPPER_INTERNAL_H
#define UPNP_PORT_MAPPER_INTERNAL_H

#include "upnp_port_mapper.h"

//! mutex protected callback
class UpnpPortMapper::CallbackControl
{
public:
    CallbackControl( const std::function< void( const MappingInfo& ) >& callback );
    void call( const MappingInfo& info );
    void clear();
    MappingInfo state();

private:
    QMutex m_mutex;
    MappingInfo m_state;
    std::function< void( const MappingInfo& ) > m_callback;
};

//! slowes down requests in case of failures
class FailCounter
{
public:
    FailCounter();
    void success();
    void failure();
    bool isOk();

private:
    size_t m_failsInARow;
    size_t m_lastFail;
};

//! single mapping device interface
class UpnpPortMapper::MappingDevice
{
public:
    MappingDevice( UpnpAsyncClient* upnpClient,
                   const HostAddress& internalIp,
                   const QUrl& url,
                   const QString& description);

    HostAddress externalIp() const;

    bool map( quint16 port, quint16 desiredPort, Protocol protocol,
              const std::function< void( quint16 ) >& callback );

    bool unmap( quint16 port, Protocol protocol,
                const std::function< void() >& callback );

private:
    bool mapImpl( quint16 port, quint16 desiredPort, Protocol protocol, size_t retrys,
                  const std::function< void( quint16 ) >& callback );

private:
    UpnpAsyncClient* const m_upnpClient;
    const HostAddress m_internalIp;
    const QUrl m_url;
    const QString m_description;

    mutable QMutex m_mutex;
    HostAddress m_externalIp;
    FailCounter m_faultCounter;

    // result are waiting here while m_externalIp is not avaliable
    std::vector< std::function< void() > > m_successQueue;
};

#endif // UPNP_PORT_MAPPER_INTERNAL_H
