#ifndef cl_net_state_439
#define cl_net_state_439

#include <QHostAddress>
#include <QNetworkAddressEntry>

struct CLSubNetState
{
    QHostAddress minHostAddress;
    QHostAddress maxHostAddress;
    QHostAddress currHostAddress;
};

class QN_EXPORT CLNetState
{
    typedef QMap<QString, CLSubNetState> StateMAP;
public:
    CLNetState();

    // check if it has SubNetState with such NIC machine_ip
    bool existsSubnet(const QHostAddress& machine_ip) const;

    // return CLSubNetState for NIC ipv4 machine_ip 
    CLSubNetState& getSubNetState(const QHostAddress& machine_ip);

    // check if addr is in the same subnet as one of our IPV4 interface 
    bool isAddrInMachineSubnet(const QHostAddress& addr) const;

    // checks if resource with such hostaddr and discover addr
    bool isResourceInMachineSubnet(const QHostAddress& addr, const QHostAddress& discAddr) const;

    // this function updates all IPV4 subnets;
    // if it gets new subnet => m_netstate gonna be increased
    // if some subnet diapered since last call =>  m_netstate will be decreased
    void updateNetState();

    QString toString() const;

private:    
    QList<QNetworkAddressEntry> m_net_entries;
    StateMAP m_netstate; // net state for each subnet
};

#endif //cl_net_state_439
