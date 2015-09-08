#include "dns_table.h"

namespace nx {
namespace cc {

void DnsTable::addPeerAddress(
    const QString& /*peerName*/,
    const HostAddress& /*hostAddress*/,
    const std::vector<DnsAttribute>& /*attributes*/ )
{
    //TODO #ak
}

void DnsTable::forgetPeer( const QString& /*peerName*/ )
{
    //TODO #ak
}

void DnsTable::forcePeerAddressResolved(
    const QString& /*peerName*/,
    const DnsEntry& /*dnsEntry*/ )
{
    //TODO #ak
}

DnsTable::ResolveResult DnsTable::resolveAsync(
    const HostAddress& hostName,
    std::vector<DnsEntry>* const dnsEntries,
    std::function<void( std::vector<DnsEntry> )> /*completionHandler*/ )
{
    //TODO #ak implement local network resolve

    DnsEntry dnsEntry;
    dnsEntry.addressType = hostName.isResolved() ? AddressType::regular : AddressType::cloud;
    dnsEntry.address = hostName;
    dnsEntries->push_back( std::move( dnsEntry ) );
    return ResolveResult::done;
}

} // namespace cc
} // namespace nx
