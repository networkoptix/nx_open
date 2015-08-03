#ifndef cl_net_tools_1232
#define cl_net_tools_1232

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QNetworkAddressEntry>

#include <utils/network/socket_common.h>


static const int ping_timeout = 300;

struct CLSubNetState;

typedef QList<quint32> CLIPList;

struct QnInterfaceAndAddr
{
    QnInterfaceAndAddr(QString name_, QHostAddress address_, QHostAddress netMask_, const QNetworkInterface& _netIf)
        : name(name_),
          address(address_),
          netMask(netMask_),
          netIf(_netIf)
    {
    }

    QString name;
    QHostAddress address;
    QHostAddress netMask;
    QNetworkInterface netIf;
};

QN_EXPORT QString MACToString(const unsigned char *mac);

QN_EXPORT unsigned char* MACsToByte(const QString& macs, unsigned char* pbyAddress, const char cSep);
QN_EXPORT unsigned char* MACsToByte2(const QString& macs, unsigned char* pbyAddress);

// returns list of interfaces.
// Set allowItfWithoutAddress to <true> to get list with interfaces without any ip
typedef QList<QnInterfaceAndAddr> QnInterfaceAndAddrList;
QList<QnInterfaceAndAddr> getAllIPv4Interfaces(bool allowItfWithoutAddress = false);

// returns list of IPv4 addresses of current machine. Skip 127.0.0.1 and addresses we can't bind to.
QList<QHostAddress> allLocalAddresses();

//returns list of all IPV4 QNetworkAddressEntries of current machine; this function takes time; 
QN_EXPORT QList<QNetworkAddressEntry> getAllIPv4AddressEntries();

// return true if succeded 
QN_EXPORT bool getNextAvailableAddr(CLSubNetState& state, const CLIPList& lst);

QN_EXPORT void removeARPrecord(const QHostAddress& ip);

QN_EXPORT QString getMacByIP(const QString& host, bool net = true);
QN_EXPORT QString getMacByIP(const QHostAddress& ip, bool net = true);

QN_EXPORT QHostAddress getGatewayOfIf(const QString& netIf);

// returns all pingable hosts in the range
QN_EXPORT QList<QHostAddress> pingableAddresses(const QHostAddress& startAddr, const QHostAddress& endAddr, int threads);

//QN_EXPORT bool bindToInterface(QUdpSocket& sock, const QnInterfaceAndAddr& iface, int port = 0, QUdpSocket::BindMode mode = QUdpSocket::DefaultForPlatform);

QN_EXPORT bool isIpv4Address(const QString& addr);
QN_EXPORT QHostAddress resolveAddress(const QString& addr);

QN_EXPORT int strEqualAmount(const char* str1, const char* str2);
QN_EXPORT bool isNewDiscoveryAddressBetter(
    const HostAddress& host,
    const QHostAddress& newAddress,
    const QHostAddress& oldAddress );

static const int MAC_ADDR_LEN = 18;
/*!
    \param host If function succeeds \a *host contains pointer to statically-allocated buffer, 
        so it MUST NOT be freed!
    \return 0 on success, -1 in case of error. Use errno to get error code
*/
QN_EXPORT int getMacFromPrimaryIF(char  MAC_str[MAC_ADDR_LEN], char** host);
QN_EXPORT QString getMacFromPrimaryIF();

#endif //cl_net_tools_1232
