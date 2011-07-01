#include "ping.h"
#include "../base/log.h"

#ifdef _WIN32
#include <stdio.h>


#else
#include <SystemConfiguration/SCNetworkReachability.h>
#endif


#ifdef _WIN32
CLPing::CLPing()
{
}

bool CLPing::ping(const QString& ip, int retry, int timeoutPerRetry, int packetSize)
{
	// Declare and initialize variables

	HANDLE hIcmpFile;
	unsigned long ipaddr = INADDR_NONE;
	DWORD dwRetVal = 0;
	DWORD dwError = 0;
	char SendData[] = "Data Buffer";

	const DWORD ReplySize = sizeof (ICMP_ECHO_REPLY) + sizeof (SendData) + 8;;
	LPVOID ReplyBuffer[ReplySize];

	ipaddr = inet_addr(ip.toLatin1().data());
	if (ipaddr == INADDR_NONE) 
		return false;

	hIcmpFile = IcmpCreateFile();
	if (hIcmpFile == INVALID_HANDLE_VALUE) 
	{
		cl_log.log("CLPing: Unable to open handle ", cl_logERROR);
		//printf("\tUnable to open handle.\n");
		//printf("IcmpCreatefile returned error: %ld\n", GetLastError());
		return false;
	}

	// Allocate space for at a single reply
	dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof (SendData), NULL,
		ReplyBuffer, ReplySize, retry * timeoutPerRetry);
	if (dwRetVal != 0) 
	{
		PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY) ReplyBuffer;
		struct in_addr ReplyAddr;
		ReplyAddr.S_un.S_addr = pEchoReply->Address;

		/*/
		if (dwRetVal > 1) 
		{
			printf("\tReceived %ld icmp message responses\n", dwRetVal);
			printf("\tInformation from the first response:\n");
		} 
		else 
		{
			printf("\tReceived %ld icmp message response\n", dwRetVal);
			printf("\tInformation from this response:\n");
		}

		printf("\t  Received from %s\n", inet_ntoa(ReplyAddr));
		printf("\t  Status = %ld  ", pEchoReply->Status);
		/**/

		switch (pEchoReply->Status) 
		{
		case IP_DEST_HOST_UNREACHABLE:
			//printf("(Destination host was unreachable)\n");
			return false;
			break;
		case IP_DEST_NET_UNREACHABLE:
			//printf("(Destination Network was unreachable)\n");
			return false;
			break;
		case IP_REQ_TIMED_OUT:
			//printf("(Request timed out)\n");
			return false;
			break;
		default:
			//printf("\n");
			return true;
			break;
		}

		//printf("\t  Roundtrip time = %ld milliseconds\n",pEchoReply->RoundTripTime);
	} 
	else 
	{
		cl_log.log(ip + " CLPing: Call to IcmpSendEcho failed", cl_logERROR);

		printf("Call to IcmpSendEcho failed.\n");
		dwError = GetLastError();
		switch (dwError) {
		case IP_BUF_TOO_SMALL:
            cl_log.log("CLPing: tReplyBufferSize to small", cl_logERROR);
			//printf("\tReplyBufferSize to small\n");
			break;
		case IP_REQ_TIMED_OUT:
            cl_log.log("CLPing: \tRequest timed out", cl_logERROR);
			//printf("\tRequest timed out\n");
			break;
		default:
            cl_log.log("CLPing: \tExtended error returned ", (int)dwError, cl_logERROR);
			//printf("\tExtended error returned: %ld\n", dwError);
			break;
		}
		/**/

		return false;
	}
}
#else

CLPing::CLPing()
{
}

bool CLPing::ping(const QString& ip, int retry, int timeoutPerRetry, int packetSize)
{
	SCNetworkReachabilityRef target;
	SCNetworkConnectionFlags flags = 0;
	Boolean ok;
	target = SCNetworkReachabilityCreateWithName(NULL, ip.toLatin1().data());
	ok = SCNetworkReachabilityGetFlags(target, &flags);
	CFRelease(target);
	
	return ok;
}

#endif
