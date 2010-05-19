#include "netstate.h"
#include <QMutexLocker>
#include "nettools.h"


CLNetState::CLNetState()
{
	init();
}

void CLNetState::init()
{
	m_net_entries = getAllIPv4AddressEntries(false);
	for (int i = 0; i < m_net_entries.size(); ++i)
	{
		const QNetworkAddressEntry& entry = m_net_entries.at(i);

		quint32 ip = entry.ip().toIPv4Address();
		quint32 mask = entry.netmask().toIPv4Address();

		quint32 minaddr = ip & mask;
		quint32 maxaddr = ip | (~mask);

		CLSubNetState state;

		state.currHostAddress = QHostAddress(minaddr);
		state.minHostAddress = QHostAddress(minaddr);
		state.maxHostAddress = QHostAddress(maxaddr);

		m_netstate[entry.ip().toString()] = state;
	}
}

QString CLNetState::toString() const
{

	QString result;

	QTextStream str(&result);

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


bool CLNetState::exists(const QHostAddress& machine_ip) const
{
	return (m_netstate.find(machine_ip.toString())!=m_netstate.end());
}

// return CLSubNetState for NIC ipv4 machine_ip 
CLSubNetState& CLNetState::getSubNetState(const QHostAddress& machine_ip)
{
	return m_netstate[machine_ip.toString()];
}

void CLNetState::setSubNetState(const QHostAddress& machine_ip, const CLSubNetState& sn)
{
	m_netstate[machine_ip.toString()] = sn;
}

bool CLNetState::isInMachineSubnet(const QHostAddress& addr) const
{
	for (int i = 0; i < m_net_entries.size(); ++i)
	{
		const QNetworkAddressEntry& entr = m_net_entries.at(i);
		if (entr.ip().isInSubnet(addr, entr.prefixLength()))
			return true;

	}
	return false;

}

const QList<QNetworkAddressEntry>&  CLNetState::getAllIPv4AddressEntries(bool from_memorry )
{
	//QMutexLocker lock(&m_mutex);

	if (from_memorry)
		return m_net_entries;


	m_net_entries = ::getAllIPv4AddressEntries();

	return m_net_entries;
}

