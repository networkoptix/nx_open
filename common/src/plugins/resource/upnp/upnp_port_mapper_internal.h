#ifndef UPNP_PORT_MAPPER_INTERNAL_H
#define UPNP_PORT_MAPPER_INTERNAL_H

#include "upnp_port_mapper.h"

//! mutex protected callback
class UpnpPortMapper::CallbackControl
{
public:
    CallbackControl( const MappingCallback& callback );
    void call( const MappingInfo& info );
    void clear();

private:
    QMutex m_mutex;
    MappingCallback m_callback;
};

//! single mapping device interface
class UpnpPortMapper::MappingDevice
{
public:
    MappingDevice( UpnpAsyncClient* upnpClient,
                   const HostAddress& internalIp,
                   const QUrl& url);

    HostAddress externalIp() const;

    void map( quint16 port, quint16 desiredPort,
              const std::function< void( quint16 ) >& callback );

    void unmap( quint16 port, const std::function< void() >& callback );

private:
    UpnpAsyncClient* const m_upnpClient;
    const HostAddress m_internalIp;
    const QUrl m_url;

    mutable QMutex m_mutex;
    HostAddress m_externalIp;
    // result are waiting here while m_externalIp is not avaliable
    std::map< quint16, std::function< void() > > m_successQueue;
};

#endif // UPNP_PORT_MAPPER_INTERNAL_H
