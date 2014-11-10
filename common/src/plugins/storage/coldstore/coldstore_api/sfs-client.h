////////////////////////////////////////////////////////////////////////////////
//
// sfs-client.h
//
// File version: 1.22
// SDK  version: 1.1.0
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __VERACITY_SFS_CLIENT_H
#define __VERACITY_SFS_CLIENT_H

#if defined Q_OS_WIN
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#endif

#include "ISFS.h"

namespace Veracity
{
#ifndef ntohll
	u64 ntohll(u64 v);
#endif
#ifndef htonll
	u64 htonll(u64 v);
#endif

	class TCPConnection
	{
	public:
		TCPConnection();
		virtual ~TCPConnection();
		int GetErrorCode();
		int Connect(const char* server, int port);
		void Disconnect();
		u64 Send(const char* buffer, u64 size);
		u64 Receive(char* buffer, u64 size);

	private:
		enum { MAX_SEND_BUFFER_SIZE = 0x100000, MAX_RECEIVE_BUFFER_SIZE = 0x100000 };
		int fd;
		sockaddr_in sa;
		int error_code;

		void CloseSocket();
		void SetErrorCode();
	};

	class SFSPacket
	{
	////////////////////////////////////////////////////////////////////////////////////////
	//
	// Packet Header and Parameter Fields (all stored in network byte order)
	//
	// Offset | Size | Name       | Description
	//--------+------+-----------------+----------------------------------------------------------
	//     0  |   1  | version         | Protocol version 
	//     1  |   1  | header_size     | The size of this header
	//     2  |   1  | command         | Command (SFS_CMD_READ, SFS_CMD_WRITE, etc.)
	//     3  |   1  | parameter_count | The count of parameters following this header
	//     4  |   2  | payload_offset  | Offset from the start of the packet to the start of the payload
	//     6  |   2  | spare           | Currently unused
	//     8  |   8  | payload_length  | Payload length
	//    16  | var  | parameters      | Parameters index followed by parameter values
	//
	////////////////////////////////////////////////////////////////////////////////////////
	public:
		enum { VERSION = 3, HEADER_SIZE = 16 };
		enum
		{
			PARAMETER_NULL				= 0x00,
			PARAMETER_CHANNEL			= 0x01,
			PARAMETER_FILE_NAME			= 0x02,
			PARAMETER_FILE_SIZE			= 0x03,
			PARAMETER_FILE_POSITION		= 0x04,
			PARAMETER_FILE_OFFSET		= 0x05,
			PARAMETER_TIME_START		= 0x06,
			PARAMETER_TIME_END			= 0x07,
			PARAMETER_TIME_CURRENT		= 0x08,
			PARAMETER_DATA_TAG			= 0x09,
			PARAMETER_DATA_LENGTH		= 0x0a,
			PARAMETER_SEEK_WHENCE		= 0x0b,
			PARAMETER_SEEK_DIRECTION	= 0x0c,
			PARAMETER_STREAM_NO			= 0x0d,
			PARAMETER_STATUS			= 0x0e,
			PARAMETER_CLIENT_ID			= 0x0f,
			PARAMETER_FILE_COUNT		= 0x10,
			PARAMETER_DISK_SIZE			= 0x11,
			PARAMETER_BUFFER_SIZE		= 0x12,
			PARAMETER_BYTES_REMAINING	= 0x13,
			PARAMETER_STRING_START		= 0x20,
			PARAMETER_STRING_LENGTH		= 0x21,
			PARAMETER_FILE_QUERY_FLAGS	= 0x22
		};
		enum
		{
			SFS_CMD_OPEN			= 0x01,
			SFS_CMD_CREATE			= 0x03,
			SFS_CMD_WRITE			= 0x04,
			SFS_CMD_READ			= 0x05,
			SFS_CMD_SEEK			= 0x06,
			SFS_CMD_CLOSE			= 0x07,
			SFS_CMD_ECHO			= 0x08,
			SFS_CMD_CONNECT			= 0x09,
			SFS_CMD_TELL			= 0x19,
			SFS_CMD_ERASEALL		= 0x1a,
			SFS_CMD_BULK_READ		= 0x1b,
			SFS_CMD_FILE_QUERY		= 0x1d
		};
		enum
		{
			FQF_DISTINCT				= 0x00000001,
			FQF_FILENAME_USE_WILDCARDS	= 0x00000002,
			FQF_ORDER_DESCENDING		= 0x00000004,
			FQF_RETURN_CHANNEL			= 0x00000010,
			FQF_RETURN_FILENAME			= 0x00000020,
			FQF_RETURN_FIRST_MS			= 0x00000040
		};

		void	SetVersion(u8 value)		{ buffer[0] = value; }
		u8		GetVersion() const			{ return buffer[0]; }
		void	SetHeaderSize(u8 value)		{ buffer[1] = value; }
		u8		GetHeaderSize() const		{ return buffer[1]; }
		void	SetCommand(u8 value)		{ buffer[2] = value; }
		u8		GetCommand() const			{ return buffer[2]; }
		void	SetParameterCount(u8 value)	{ buffer[3] = value; }
		u8		GetParameterCount() const	{ return buffer[3]; }
		void	SetPayloadOffset(u16 value)	{ *((u16*)(buffer + 4)) = htons(value); }
		u16		GetPayloadOffset() const	{ return ntohs(*((u16*)(buffer + 4))); }
		void	SetSpare(u16 value)			{ *((u16*)(buffer + 6)) = htons(value); }
		u16		GetSpare() const			{ return ntohs(*((u16*)(buffer + 6))); }
		void	SetPayloadLength(u64 value)	{ *((u64*)(buffer + 8)) = htonll(value); }
		u64		GetPayloadLength()			{ return ntohll(*((u64*)(buffer + 8))); }

		void	SetBuffer(char* value)		{ buffer = value; }
		char*	GetBuffer() const			{ return buffer; }

		void Clear();
		void AddParameter(int id, const void* value);
		void ResetParameterIterator();
		int GetNextParameter(int* id, void** value);

	private:
		static const int parameter_size[20];
		
		char* buffer;
		int parameter_current;
		int parameter_index;
	};

	class SFSNetworkSession
	{
	public:
		SFSNetworkSession();
		virtual ~SFSNetworkSession();
		int Start(const char* server, u16 port);
		void Stop();
		SFSPacket& GetPacket();
		int GetErrorCode();
		u32 GetMaximumPayloadBufferSize();
		int ReceivePacket(char* payload_buffer, u64 payload_maxsize);
		int SendPacket(const char* payload_buffer);

	private:
		enum { NETWORK_BUFFER_SIZE = 0x400 };
		TCPConnection connection;
		SFSPacket packet;
		char* network_buffer;
		int error_code;

	};

	class SFSClient : public ISFS
	{
	public:
        SFSClient();
        virtual ~SFSClient();
		u32 Connect(const char* server, u16 port = DEFAULT_PORT);
		u32 Disconnect();
		u32 Echo();
		u32 Open(const char* file_name,
				 u64 time_current,
				 u32 channel,
				 u32* returned_stream_no,
				 char* returned_file_name = 0,
				 u64* returned_file_size = 0,
				 u64* returned_file_position = 0,
				 u64* returned_time_current = 0);
		u32 Create(const char* file_name, 
				   u32 channel, 
				   u32* returned_stream_no,
				   u64* returned_time_current = 0);
		u32 Close(u32 stream_no);
		u32 Read(u32 stream_no,
				 void* buf,
				 u64 buffer_size,
				 u64 data_length,
				 u64* returned_data_length = 0,
				 u64* returned_data_remaining = 0);
		u32 Read(u32 stream_no,
				 void* buf,
				 u64 buffer_size,
				 u64 time_start,
				 u64 time_end,
				 u64* returned_data_length = 0,
				 u64* returned_bytes_remaining = 0,
				 u64* returned_time_current = 0,
				 u64* returned_file_position = 0);
		u32 Write(u32 stream_no,
				  const void* buf,
				  u64 data_length,
				  u32 data_tag,
				  u64* returned_time_current = 0,
				  u64* returned_data_length = 0);
		u32 Tell(u32 stream_no,
				 u64* returned_time_current,
				 u64* returned_file_position,
				 u64* returned_file_size,
				 u64* returned_time_start = 0,
				 u64* returned_time_end = 0);
		u32 Seek(u32 stream_no,
				 s64 file_offset,
				 u8 seek_whence,
				 u64 time_start,
				 u32 data_tag,
				 u8 seek_direction,
				 u64* returned_time_current = 0,
				 u64* returned_file_position = 0,
				 u32* returned_data_tag = 0);
		u32 FileQuery(bool distinct,
					  bool return_channel,
					  bool return_filename,
					  bool return_first_ms,
					  u32 string_start,
					  u32 string_length,
                      const char* file_name,
					  u32 channel,
					  bool use_channel,
					  u32 limit,
					  bool order_descending,
					  u64 time_start,
					  char* results,
					  u32* returned_results_size,
					  u32* returned_result_count);
		u32 EraseAll(u64 disk_size = (u64)-1);
		int GetNetworkErrorCode();

	protected:
		SFSNetworkSession network_session;
		int error_code;
	};
};

#endif
