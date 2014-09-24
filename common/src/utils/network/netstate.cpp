#include "netstate.h"
#include "nettools.h"
#include "../common/log.h"


CLNetState::CLNetState()
{
    updateNetState();
}

void CLNetState::updateNetState()
{
    //bool net_state_changed = false;

    m_net_entries = getAllIPv4AddressEntries();

    StateMAP new_netstate;

    for (int i = 0; i < m_net_entries.count(); ++i)
    {
        const QNetworkAddressEntry& entry = m_net_entries.at(i);

        quint32 ip = entry.ip().toIPv4Address();
        quint32 mask = entry.netmask().toIPv4Address();

        //QString ip_s = entry.ip().toString(); //debug
        //QString mask_s = entry.netmask().toString(); //debug

        quint32 minaddr = ip & mask;
        quint32 maxaddr = ip | (~mask);

        CLSubNetState state;

        state.currHostAddress = QHostAddress(minaddr);
        state.minHostAddress = QHostAddress(minaddr);
        state.maxHostAddress = QHostAddress(maxaddr);

        if ( existsSubnet(entry.ip()) ) // mostly to check if net state changed
        {
            CLSubNetState& existing_state = getSubNetState(entry.ip());

            if (state.minHostAddress == existing_state.minHostAddress && state.maxHostAddress == existing_state.maxHostAddress)// same subnet, nothing changed
                state.currHostAddress = existing_state.currHostAddress;
          //  else// subnet mask changed
          //     net_state_changed = true;

        }
        //else
        //    net_state_changed = true; // ip changed or new subnet

        new_netstate[entry.ip().toString()] = state;
    }

    /*if (new_netstate.size()!=m_netstate.size())
        net_state_changed = true;*/

    m_netstate = new_netstate;

    /*
    if (net_state_changed)
    {
        NX_LOG(QLatin1String("Network interface list:"), cl_logINFO);
        NX_LOG(toString(), cl_logINFO);
    }
    */
}

QString CLNetState::toString() const
{

    QString result;

    QTextStream str(&result);

    str << endl;
    str << endl;

    for (int i = 0; i < m_net_entries.size(); ++i)
    {
        QNetworkAddressEntry entr = m_net_entries.at(i);

        QString ip = entr.ip().toString();
        QString mask = entr.netmask().toString();

        str <<"ip = "<< ip << ";  mask = "  << mask  << ";  low_addr = " << m_netstate[ip].minHostAddress.toString() << ";  hi_addr = " << m_netstate[ip].maxHostAddress.toString() <<";" << endl;

    }

    return result;

}

bool CLNetState::existsSubnet(const QHostAddress& machine_ip) const
{
    return (m_netstate.contains(machine_ip.toString()));
}

// return CLSubNetState for NIC ipv4 machine_ip
CLSubNetState& CLNetState::getSubNetState(const QHostAddress& machine_ip)
{
    return m_netstate[machine_ip.toString()];
}

bool CLNetState::isAddrInMachineSubnet(const QHostAddress& addr) const
{
    for (int i = 0; i < m_net_entries.size(); ++i)
    {
        const QNetworkAddressEntry& entr = m_net_entries.at(i);

        QString addr_str = addr.toString();
        QString entr_ip_str = entr.ip().toString();
        QString entr_mask = entr.netmask().toString();
        if (entr.ip().isInSubnet(addr, entr.prefixLength()))
        //if (entr.ip().isInSubnet(addr, entr.netmask().toIPv4Address()))
            return true;

    }
    return false;

}

bool CLNetState::isResourceInMachineSubnet(const QString& addr, const QHostAddress& discAddr) const
{
    return isResourceInMachineSubnet(resolveAddress(addr), discAddr);
}

bool CLNetState::isResourceInMachineSubnet(const QHostAddress& addr, const QHostAddress& discAddr) const
{
    for (int i = 0; i < m_net_entries.size(); ++i)
    {
        const QNetworkAddressEntry& entr = m_net_entries.at(i);

        QString addr_str = addr.toString();
        QString discAddr_str = discAddr.toString();
        QString entr_ip_str = entr.ip().toString();
        QString entr_mask = entr.netmask().toString();
        if (entr.ip().isInSubnet(addr, entr.prefixLength()) && entr.ip().isInSubnet(discAddr, entr.prefixLength()))
            //if (entr.ip().isInSubnet(addr, entr.netmask().toIPv4Address()))
            return true;

    }
    return false;
}
