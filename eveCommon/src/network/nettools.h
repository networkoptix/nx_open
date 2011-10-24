#ifndef cl_net_tools_1232
#define cl_net_tools_1232

#include <QHostAddress>
#include <QNetworkAddressEntry>

struct CLSubNetState;

typedef QList<quint32> CLIPList;

QString MACToString (const unsigned char* mac);
unsigned char* MACsToByte(const QString& macs, unsigned char* pbyAddress);

// returns list of IPv4 addresses of current machine
QList<QHostAddress> getAllIPv4Addresses();

//returns list of all IPV4 QNetworkAddressEntries of current machine; this function takes time; 
QList<QNetworkAddressEntry> getAllIPv4AddressEntries();

// return true if succeded 
bool getNextAvailableAddr(CLSubNetState& state, const CLIPList& lst);

void removeARPrecord(const QHostAddress& ip);

QString getMacByIP(const QHostAddress& ip, bool net = true);

// returns all pingable hosts in the range
QList<QHostAddress> pingableAddresses(const QHostAddress& startAddr, const QHostAddress& endAddr, int threads);

#endif //cl_net_tools_1232
