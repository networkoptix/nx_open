#ifndef cl_net_tools_1232
#define cl_net_tools_1232

#include <QHostAddress>
#include <QNetworkAddressEntry>

static const int ping_timeout = 300;

struct CLSubNetState;

typedef QList<quint32> CLIPList;

QN_EXPORT QString MACToString (const unsigned char* mac);

QN_EXPORT unsigned char* MACsToByte(const QString& macs, unsigned char* pbyAddress);
QN_EXPORT unsigned char* MACsToByte2(const QString& macs, unsigned char* pbyAddress);

// returns list of IPv4 addresses of current machine
QN_EXPORT QList<QHostAddress> getAllIPv4Addresses();

// returns list of IPv4 addresses of current machine. Skip 127.0.0.1 and addresses we can't bind to.
QList<QHostAddress> allLocalAddresses();

//returns list of all IPV4 QNetworkAddressEntries of current machine; this function takes time; 
QN_EXPORT QList<QNetworkAddressEntry> getAllIPv4AddressEntries();

// return true if succeded 
QN_EXPORT bool getNextAvailableAddr(CLSubNetState& state, const CLIPList& lst);

QN_EXPORT void removeARPrecord(const QHostAddress& ip);

QN_EXPORT QString getMacByIP(const QHostAddress& ip, bool net = true);

// returns all pingable hosts in the range
QN_EXPORT QList<QHostAddress> pingableAddresses(const QHostAddress& startAddr, const QHostAddress& endAddr, int threads);

#endif //cl_net_tools_1232
