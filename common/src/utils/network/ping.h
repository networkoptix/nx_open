#ifndef ping_1750
#define ping_1750

/**
  * Max ICMP packet size
  */
#define CL_MAX_PING_PACKET 1024 

class QN_EXPORT CLPing
{
public:
    CLPing();
    bool ping(const QString& ip, int retry, int timeoutPerRetry, int packetSize = 32);

private:
    char m_icmpData[CL_MAX_PING_PACKET];
    char m_receiveBuffer[CL_MAX_PING_PACKET];
};

#endif //ping_1750
