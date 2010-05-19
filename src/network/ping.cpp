#include "ping.h"


#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#define IP_RECORD_ROUTE  0x7
// 
// IP header structure
//
typedef struct _iphdr 
{
	unsigned int   h_len:4;        // Length of the header
	unsigned int   version:4;      // Version of IP
	unsigned char  tos;            // Type of service
	unsigned short total_len;      // Total length of the packet
	unsigned short ident;          // Unique identifier
	unsigned short frag_and_flags; // Flags
	unsigned char  ttl;            // Time to live
	unsigned char  proto;          // Protocol (TCP, UDP, etc.)
	unsigned short checksum;       // IP checksum

	unsigned int   sourceIP;
	unsigned int   destIP;
} IpHeader;

#define ICMP_ECHO        8
#define ICMP_ECHOREPLY   0
#define ICMP_MIN         8 // Minimum 8-byte ICMP packet (header)

//
// ICMP header structure
//
typedef struct _icmphdr 
{
	BYTE   i_type;
	BYTE   i_code;                 // Type sub code
	USHORT i_cksum;
	USHORT i_id;
	USHORT i_seq;
	// This is not the standard header, but we reserve space for time
	ULONG  timestamp;
} IcmpHeader;

//
// IP option header--use with socket option IP_OPTIONS
//
typedef struct _ipoptionhdr
{
	unsigned char        code;        // Option type
	unsigned char        len;         // Length of option hdr
	unsigned char        ptr;         // Offset into options
	unsigned long        addr[9];     // List of IP addrs
} IpOptionHeader;

#define DEF_PACKET_SIZE  32        // Default packet size
#define MAX_IP_HDR_SIZE  60        // Max IP header size w/options


void FillICMPData(char *icmp_data, int datasize)
{
	IcmpHeader *icmp_hdr = NULL;
	char       *datapart = NULL;

	icmp_hdr = (IcmpHeader*)icmp_data;
	icmp_hdr->i_type = ICMP_ECHO;        // Request an ICMP echo
	icmp_hdr->i_code = 0;
	icmp_hdr->i_id = (USHORT)GetCurrentProcessId();
	icmp_hdr->i_cksum = 0;
	icmp_hdr->i_seq = 0;

	datapart = icmp_data + sizeof(IcmpHeader);
	//
	// Place some junk in the buffer
	//
	memset(datapart,'E', datasize - sizeof(IcmpHeader));
}

// 
// Function: checksum
//
// Description:
//    This function calculates the 16-bit one's complement sum
//    of the supplied buffer (ICMP) header
//
USHORT checksum(USHORT *buffer, int size) 
{
	unsigned long cksum=0;

	while (size > 1) 
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	if (size) 
	{
		cksum += *(UCHAR*)buffer;
	}
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >>16);
	return (USHORT)(~cksum);
}



//
// Function: DecodeICMPHeader
//
// Description:
//    The response is an IP packet. We must decode the IP header to
//    locate the ICMP data.
//
bool DecodeICMPHeader(char *buf, int bytes, struct sockaddr_in *from)
{
	IpHeader       *iphdr = NULL;
	IcmpHeader     *icmphdr = NULL;
	unsigned short  iphdrlen;
	DWORD           tick;


	iphdr = (IpHeader *)buf;
	// Number of 32-bit words * 4 = bytes
	iphdrlen = iphdr->h_len * 4;

	if (bytes  < iphdrlen + ICMP_MIN) 
		return false;

	icmphdr = (IcmpHeader*)(buf + iphdrlen);

	if (icmphdr->i_type != ICMP_ECHOREPLY) 
		return false;


	// Make sure this is an ICMP reply to something we sent!
	//
	if (icmphdr->i_id != (USHORT)GetCurrentProcessId()) 
		return false;

	return true;
}

CLPing::CLPing()
{
	static bool initialized = false;
	if (!initialized) 
	{
		WORD wVersionRequested;
		WSADATA wsaData;

		wVersionRequested = MAKEWORD(2, 0);              // Request WinSock v2.0
		if (WSAStartup(wVersionRequested, &wsaData) != 0) {  // Load WinSock DLL
			return;
		}
		initialized = true;
	}

}


bool CLPing::ping(const QString& ip, int retry, int m_timeout, int datasize)
{
	// Description:
	//    Set up the ICMP raw socket, and create the ICMP header. Add
	//    the appropriate IP option header, and start sending ICMP
	//    echo requests to the endpoint. For each send and receive,
	//    we set a timeout value so that we don't wait forever for a 
	//    response in case the endpoint is not responding. When we
	//    receive a packet, decode it.
	//


	SOCKET             sockRaw = INVALID_SOCKET;
	struct sockaddr_in dest,
		from;
	int                bread,
		fromlen = sizeof(from),
		timeout = m_timeout,
		ret;
	

	unsigned int       addr = 0;
	USHORT             seq_no = 0;
	struct hostent    *hp = NULL;
	IpOptionHeader     ipopt;


	//
	// WSA_FLAG_OVERLAPPED flag is required for SO_RCVTIMEO, 
	// SO_SNDTIMEO option. If NULL is used as last param for 
	// WSASocket, all I/O on the socket is synchronous, the 
	// internal user mode wait code never gets a chance to 
	// execute, and therefore kernel-mode I/O blocks forever. 
	// A socket created via the socket function has the over-
	// lapped I/O attribute set internally. But here we need 
	// to use WSASocket to specify a raw socket.
	//
	// If you want to use timeout with a synchronous 
	// nonoverlapped socket created by WSASocket with last 
	// param set to NULL, you can set the timeout by using 
	// the select function, or you can use WSAEventSelect and 
	// set the timeout in the WSAWaitForMultipleEvents 
	// function.
	//
	sockRaw = WSASocket (AF_INET, SOCK_RAW, IPPROTO_ICMP, NULL, 0,
		WSA_FLAG_OVERLAPPED);
	if (sockRaw == INVALID_SOCKET) 
		return false;



	// Set the send/recv timeout values
	//
	bread = setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, 
		(char*)&timeout, sizeof(timeout));
	if(bread == SOCKET_ERROR) 
		return false;

	timeout = m_timeout;
	bread = setsockopt(sockRaw, SOL_SOCKET, SO_SNDTIMEO, 
		(char*)&timeout, sizeof(timeout));

	if (bread == SOCKET_ERROR) 
		return false;

	memset(&dest, 0, sizeof(dest));
	//
	// Resolve the endpoint's name if necessary
	//
	dest.sin_family = AF_INET;
	if ((dest.sin_addr.s_addr = inet_addr(ip.toLatin1().data())) == INADDR_NONE)
	{
		if ((hp = gethostbyname(ip.toLatin1().data())) != NULL)
		{
			memcpy(&(dest.sin_addr), hp->h_addr, hp->h_length);
			dest.sin_family = hp->h_addrtype;
		}
		else
			return false;

	}        

	// 
	// Create the ICMP packet
	//       
	datasize += sizeof(IcmpHeader);  


	memset(icmp_data,0,CL_MAX_PING_PACKET);
	FillICMPData(icmp_data,datasize);
	//
	// Start sending/receiving ICMP packets
	//

	int attempt = 0;
	while(1) 
	{
		
		attempt++;
		if (attempt>retry) return false;


		int bwrote;


		((IcmpHeader*)icmp_data)->i_cksum = 0;
		((IcmpHeader*)icmp_data)->timestamp = GetTickCount();
		((IcmpHeader*)icmp_data)->i_seq = seq_no++;
		((IcmpHeader*)icmp_data)->i_cksum = 
			checksum((USHORT*)icmp_data, datasize);

		bwrote = sendto(sockRaw, icmp_data, datasize, 0, 
			(struct sockaddr*)&dest, sizeof(dest));
		if (bwrote == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAETIMEDOUT) 
				continue;

			return false;
		}



		bread = recvfrom(sockRaw, recvbuf, CL_MAX_PING_PACKET, 0, 
			(struct sockaddr*)&from, &fromlen);

		if (bread == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAETIMEDOUT) 
				continue;

			return false;
		}

		
		if (DecodeICMPHeader(recvbuf, bread, &from))
		{
			if (sockRaw != INVALID_SOCKET) 
				closesocket(sockRaw);

			return true;
		}

	}

	if (sockRaw != INVALID_SOCKET) 
		closesocket(sockRaw);



	return false;
}


