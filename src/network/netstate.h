#ifndef cl_net_state_439
#define cl_net_state_439

#include <QMap>
#include <QMutex>
#include <QList>
#include <QNetworkAddressEntry>


struct CLSubNetState
{
	QHostAddress minHostAddress;
	QHostAddress maxHostAddress;
	QHostAddress currHostAddress;
};


class CLNetState
{
	typedef QMap<QString, CLSubNetState> StateMAP;
public:
	CLNetState();

	// check if it has SubNetState with such NIC machine_ip
	bool exists(const QHostAddress& machine_ip) const;

	// return CLSubNetState for NIC ipv4 machine_ip 
	CLSubNetState& getSubNetState(const QHostAddress& machine_ip);

	void setSubNetState(const QHostAddress& machine_ip, const CLSubNetState& sn);

	const QList<QNetworkAddressEntry>&  getAllIPv4AddressEntries(bool from_memorry = true);

	// check if addr is in the same subnet as one of our IPV4 interface 
	bool isInMachineSubnet(const QHostAddress& addr) const;


	void init();

	QString toString() const;
private:

	

	StateMAP m_netstate;
	QList<QNetworkAddressEntry> m_net_entries;

	//mutable QMutex m_mutex;

};

#endif //cl_net_state_439