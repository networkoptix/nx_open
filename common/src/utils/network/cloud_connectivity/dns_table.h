/**********************************************************
* 29 aug 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_DNS_TABLE_H
#define NX_DNS_TABLE_H

#include "../socket_common.h"


//!Types used in resolving peer names
/*!
    \note It is not a DNS implementation! It is something proprietary used to get network address of NX peer
*/
namespace nx_cc
{
    enum class AddressType
    {
        regular,
        cloud,       //!< Address that requires using mediator
        unknown
    };

    struct DnsEntry
    {
        AddressType addressType;
        HostAddress address;

        DnsEntry()
        :
            addressType( AddressType::unknown )
        {
        }
    };

    enum class AttributeType
    {
        //!nx peer reported its address and name by itself
        peerFoundPassively,
        //!port of NX peer (server port)
        nxApiPort
    };

    struct DnsAttribute
    {
        AttributeType type;
        QVariant value;
    };

    //!Contains peer names, their known addresses and some attributes
    /*!
        
    */
    class DnsTable
    {
    public:
        //!Add new peer address
        /*!
            Peer addresses are resolved from time to time in the following way:\n
            - custom NX resolve request is sent if \a nxApiPort attribute is provided. If peer responds and reports same name as \a peerName than address considered "resolved"
            - if host does not respond to custom NX resolve request or no \a nxApiPort attribute, than ping is used from time to time
            - resolved address is "resolved" for only some period of time

            \param attributes Attributes refer to \a hostAddress not \a peerName

            \note Peer can have multiple addresses
        */
        void addPeerAddress(
            const QString& peerName,
            const HostAddress& hostAddress,
            const std::vector<DnsAttribute>& attributes );
        //!Remove peer \a peerName and all its addresses
        void forgetPeer( const QString& peerName );
        //!Add address with specified type that will always be considered "resolved"
        void forcePeerAddressResolved( const QString& peerName, const DnsEntry& dnsEntry );

        /*!
            - if \a hostName is an ipv4 address, than that address is returned
            - if \a hostName is a known peer name, registered with \a DnsTable::addPeerAddress than if some address is resolved, it is returned. 
                Otherwise, if resolve retry timeout had passed, async resolve is started. Resolve is described in \a DnsTable::addPeerAddress

            \param dnsEntries If \a hostName can be resolved immediately, it is done and result placed to \a dnsEntries. 
                Otherwise, if completionHandler is specified, async resolve procedure is started

            \return \a true if resolve is done and result placed to \a dnsEntry. \a false if async resolve procedure has been initiated
        */
        bool resolveAsync(
            const HostAddress& hostName,
            std::vector<DnsEntry>* const dnsEntries,
            std::function<void(std::vector<DnsEntry>)> completionHandler = std::function<void(std::vector<DnsEntry>)>() );
        //!Calls \a DnsTable::resolveAsync and waits for completion
        std::vector<DnsEntry> resolveSync( const HostAddress& hostName );

        static DnsTable* instance();
    };
}

#endif  //NX_DNS_TABLE_H
