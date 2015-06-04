/**********************************************************
* 3 jun 2015
* a.kolesnikov
***********************************************************/

#include "dns_table.h"


namespace nx_cc
{
    void DnsTable::addPeerAddress(
        const QString& peerName,
        const HostAddress& hostAddress,
        const std::vector<DnsAttribute>& attributes )
    {
        //TODO #ak
    }

    void DnsTable::forgetPeer( const QString& peerName )
    {
        //TODO #ak
    }

    void DnsTable::forcePeerAddressResolved(
        const QString& peerName,
        const DnsEntry& dnsEntry )
    {
        //TODO #ak
    }

    DnsTable::ResolveResult DnsTable::resolveAsync(
        const HostAddress& hostName,
        std::vector<DnsEntry>* const dnsEntries,
        std::function<void( std::vector<DnsEntry> )> completionHandler )
    {
        //TODO #ak
        return ResolveResult::failed;
    }

    std::vector<DnsEntry> DnsTable::resolveSync( const HostAddress& hostName )
    {
        //TODO #ak
        return std::vector<DnsEntry>();
    }
}
