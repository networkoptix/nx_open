#ifndef ping_1750
#define ping_1750

#include <QtCore/QString>


/**
  * Max ICMP packet size
  */
#define CL_MAX_PING_PACKET 1024

class NX_NETWORK_API CLPing
{
public:
    CLPing();
    bool ping(const QString& ip, int retry, int timeoutPerRetry, int packetSize = 32);
};

#endif //ping_1750
