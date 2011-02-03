#ifndef ping_1750
#define ping_1750

#define CL_MAX_PING_PACKET       1024      // Max ICMP packet size


class CLPing
{
public:
	CLPing();
	bool ping(const QString& ip, int retry, int timeout_per_retry, int pack_size = 32);
private:

	char icmp_data[CL_MAX_PING_PACKET];
	char recvbuf[CL_MAX_PING_PACKET];
};

#endif //ping_1750
