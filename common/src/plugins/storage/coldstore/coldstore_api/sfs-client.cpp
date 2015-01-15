////////////////////////////////////////////////////////////////////////////////
//
// sfs-client.cpp
//
// File version: 1.43
// SDK  version: 1.1.0
//
////////////////////////////////////////////////////////////////////////////////

#include "sfs-client.h"
namespace Veracity
{
#ifndef ntohll
	u64 ntohll(u64 v)
	{
#ifdef VERACITY_USE_BIG_ENDIAN
		return v;
#else
		return ((u64)ntohl((u32)v) << 32) | (u64)ntohl((u32)(v >> 32));
#endif
	}
#endif

#ifndef htonll
	u64 htonll(u64 v)
	{
#ifdef VERACITY_USE_BIG_ENDIAN
		return v;
#else
		return ((u64)htonl((u32)v) << 32) | (u64)htonl((u32)(v >> 32));
#endif
	}
#endif

#pragma region TCPConnection

	TCPConnection::TCPConnection()
	{
		fd = -1;
	}

	TCPConnection::~TCPConnection()
	{
		CloseSocket();
	}

	int TCPConnection::GetErrorCode() 
	{
		return error_code;
	}

	int TCPConnection::Connect(const char* server, int port)
	{
		fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (fd < 0)
		{
			SetErrorCode();
			return -1;
		}
		int flag = 1;
		if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag)) != 0)
		{
			SetErrorCode();
			CloseSocket();
			return -1;
		}
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(server);

		sa.sin_port = htons(port);
		if (connect(fd, (sockaddr*)&sa, sizeof(sa)) != 0)
		{
			SetErrorCode();
			CloseSocket();
			return -1;
		}
		return 0;
	}

	void TCPConnection::Disconnect()
	{
		CloseSocket();
	}

	u64 TCPConnection::Send(const char* buffer, u64 size)
	{
		u64 total_bytes = 0;
		do
		{
			u64 bytes_to_send = size - total_bytes;
			if (bytes_to_send > MAX_SEND_BUFFER_SIZE)
			{
				bytes_to_send = MAX_SEND_BUFFER_SIZE;
			}
			int bytes = send(fd, buffer + total_bytes, (int)bytes_to_send, 0);
			if (bytes < 0)
			{
				SetErrorCode();
				Disconnect();
				return (u64)-1;
			}
			total_bytes += bytes;
		} while (total_bytes < size);
		return total_bytes;
	}

	u64 TCPConnection::Receive(char* buffer, u64 size)
	{
		u64 total_bytes = 0;
		do
		{
			u64 bytes_to_receive = size - total_bytes;
			if (bytes_to_receive > MAX_RECEIVE_BUFFER_SIZE)
			{
				bytes_to_receive = MAX_RECEIVE_BUFFER_SIZE;
			}
			int bytes = recv(fd, buffer + total_bytes, (int)bytes_to_receive, 0);
			if (bytes <= 0)
			{
				SetErrorCode();
				Disconnect();
				return (u64)-1;
			}
			total_bytes += bytes;
		} while (total_bytes < size);
		return total_bytes;
	}

	void TCPConnection::CloseSocket()
	{
		if (fd == -1) return;
#if defined WIN32
		shutdown(fd, SD_BOTH) ;
		closesocket(fd);
#elif defined LINUX
		shutdown(fd, SHUT_RDWR) ;
		close(fd);
#endif
		fd = -1;
	}

	void TCPConnection::SetErrorCode()
	{
#if defined WIN32
		error_code = WSAGetLastError();
#elif defined LINUX
		error_code = errno;
#endif
	}

#pragma endregion TCPConnection

#pragma region SFSPacket

	const int SFSPacket::parameter_size[20] = { 0, 4, 64, 8, 8, 8, 8, 8, 8, 4, 8, 1, 1, 4, 4, 4, 8, 8, 8, 8 };

	void SFSPacket::Clear()
	{
		memset(buffer, 0, 16);
		SetVersion(VERSION);
		SetHeaderSize(HEADER_SIZE);
		SetParameterCount(0);
		parameter_index = HEADER_SIZE;
		SetPayloadOffset(parameter_index);
	}

	void SFSPacket::AddParameter(int id, const void* value)
	{
		int size;
		if ((id >= 0x01) && (id <= 0x13))
		{
			size = parameter_size[id];
		}
		else if ((id >= 0x20) && (id < 0x30))
		{
			size = 4;
		}
		else
		{
			return;
		}
		u8 parameter_count = GetParameterCount();
		if (parameter_count == 0)
		{
			memset(buffer + HEADER_SIZE, 0, 8);
			parameter_index += 8;
		}
		else if ((parameter_count & 7) == 0)
		{
			// parameter_count is a non-zero multiple of 8.
			// We therefore need more space for the parameters index section.
			memmove(buffer + HEADER_SIZE + parameter_count + 8, buffer + HEADER_SIZE + parameter_count, 8);
			memset(buffer + HEADER_SIZE + parameter_count, 0, 8);
			parameter_index += 8;
		}
		buffer[HEADER_SIZE + parameter_count] = id;
		if (id == PARAMETER_FILE_NAME)
		{
			memset(buffer + parameter_index, 0, size);
			memcpy(buffer + parameter_index, (const char*)value, size);
			parameter_index += size;
		}
		else
		{
			switch (size)
			{
			case 1:
				buffer[parameter_index] = *((u8*)value);
				parameter_index++;
				break;
			case 2:
				*((u16*)(buffer + parameter_index)) = htons(*((u16*)value));
				parameter_index += 2;
				break;
			case 4:
				*((u32*)(buffer + parameter_index)) = htonl(*((u32*)value));
				parameter_index += 4;
				break;
			case 8:
				*((u64*)(buffer + parameter_index)) = htonll(*((u64*)value));
				parameter_index += 8;
				break;
			default:
				memcpy(buffer + parameter_index, value, size);
				parameter_index += size;
				break;
			}
		}
		SetParameterCount(++parameter_count);
		SetPayloadOffset((parameter_index + 7) & ~7);
	}

	void SFSPacket::ResetParameterIterator()
	{
		parameter_current = 0;
		parameter_index = HEADER_SIZE + ((GetParameterCount() + 7) & ~7);
	}

	int SFSPacket::GetNextParameter(int* id, void** value)
	{
		if (parameter_current >= GetParameterCount())
		{
			return -1;
		}
		*id = buffer[HEADER_SIZE + parameter_current];
		*value = (void*)(buffer + parameter_index);
		int size;
		if ((*id >= 0x01) && (*id <= 0x13))
		{
			size = parameter_size[*id];
		}
		else if ((*id >= 0x20) && (*id < 0x30))
		{
			size = 4;
		}
		else
		{
			size = 4;
		}
		parameter_index += size;
		parameter_current++;
		return 0;
	}

#pragma endregion SFSPacket

#pragma region SFSNetworkSession

	SFSNetworkSession::SFSNetworkSession()
	{
		network_buffer = new char[NETWORK_BUFFER_SIZE];
		packet.SetBuffer(network_buffer);
	}

	SFSNetworkSession::~SFSNetworkSession()
	{
		Stop();
		delete [] network_buffer;
	}

	int SFSNetworkSession::Start(const char* server, u16 port)
	{
		if (connection.Connect(server, port) == -1)
		{
			error_code = connection.GetErrorCode();
			return -1;
		}
		if (ReceivePacket(0, 0) == -1)	// Receive the connection acknowledge.
		{
			error_code = connection.GetErrorCode();
			Stop();
			return -1;
		}
		return 0;
	}

	void SFSNetworkSession::Stop()
	{
		connection.Disconnect();
	}

	SFSPacket& SFSNetworkSession::GetPacket()
	{
		return packet;
	}

	int SFSNetworkSession::GetErrorCode()
	{
		return error_code;
	}

	u32 SFSNetworkSession::GetMaximumPayloadBufferSize()
	{
		return NETWORK_BUFFER_SIZE - packet.GetPayloadOffset();
	}

	// This receives a packet (header + parameters + payload).
	// If (payload_buffer == 0) then only the header and parameters are read.
	// If (continuation == true) then the header and parameters are not read.
	int SFSNetworkSession::ReceivePacket(char* payload_buffer, u64 payload_maxsize)
	{
		// Read the packet header.
		u64 bytes_received = connection.Receive(packet.GetBuffer(), SFSPacket::HEADER_SIZE);  
		if (bytes_received == (u64)-1)
		{
			error_code = connection.GetErrorCode();
			return -1;
		}
		// Read the parameters (if there are any).
		if (packet.GetPayloadOffset() > SFSPacket::HEADER_SIZE)
		{
			bytes_received = connection.Receive(packet.GetBuffer() + SFSPacket::HEADER_SIZE, packet.GetPayloadOffset() - SFSPacket::HEADER_SIZE);
			if (bytes_received == (u64)-1)
			{
				error_code = connection.GetErrorCode();
				return -1;
			}
		}
		if (payload_buffer)
		{
			// Read the payload (if there is any).
			u64 bytes_to_receive = packet.GetPayloadLength();
			if (bytes_to_receive > 0)
			{
				if ((bytes_to_receive > payload_maxsize) && (payload_maxsize != 0))
				{
					bytes_to_receive = payload_maxsize;
				}
				u64 bytes_received = connection.Receive(payload_buffer, bytes_to_receive);
				if (bytes_received == (u64)-1)
				{
					error_code = connection.GetErrorCode();
					return -1;
				}
			}
		}
		return 0;
	}

	// This sends a packet (header + parameters + payload).
	// If the payload is small enough it should be placed in network_buffer
	// immediately after the header and the payload_buffer parameter should
	// be 0 to indicate this. The number of bytes available for the payload
	// in network_buffer is returned by GetMaximumPayloadBufferSize().
	// If the payload is too large to fit in network_buffer it should be
	// placed in its own buffer which is indicated by the payload_buffer
	// parameter.
	int SFSNetworkSession::SendPacket(const char* payload_buffer)
	{
		if (payload_buffer)
		{
			// Send payload separately as it has its own buffer.
			u64 bytes_to_send = packet.GetPayloadOffset();
			u64 bytes_sent = connection.Send(packet.GetBuffer(), bytes_to_send);
			if ((bytes_sent == (u64)-1) || (bytes_sent != bytes_to_send))
			{
				error_code = connection.GetErrorCode();
				return -1;
			}
			bytes_to_send = packet.GetPayloadLength();
			bytes_sent = connection.Send(payload_buffer, bytes_to_send);
			if ((bytes_sent == (u64)-1) || (bytes_sent != bytes_to_send))
			{
				error_code = connection.GetErrorCode();
				return -1;
			}
		}
		else
		{
			// Send the whole packet.
			u64 bytes_to_send = packet.GetPayloadOffset() + packet.GetPayloadLength();
			u64 bytes_sent = connection.Send(packet.GetBuffer(), bytes_to_send);
			if ((bytes_sent == (u64)-1) || (bytes_sent != bytes_to_send))
			{
				error_code = connection.GetErrorCode();
				return -1;
			}
		}
		return 0;
	}

#pragma endregion SFSNetworkSession

#pragma region SFSClient

	SFSClient::SFSClient()
    {
    }

    SFSClient::~SFSClient()
    {
    }

	u32 SFSClient::Connect(const char* server, u16 port)
	{
		if (network_session.Start(server, port) == -1)
		{
			error_code = network_session.GetErrorCode();
			return STATUS_NETWORK_ERROR;
		}
		return STATUS_SUCCESS;
	}

	u32 SFSClient::Disconnect()
	{
		network_session.Stop();
		return STATUS_SUCCESS;
	}

	u32 SFSClient::Echo()
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_ECHO);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		return STATUS_SUCCESS;
	}

	u32 SFSClient::Open(const char* file_name,
						u64 time_current,
						u32 channel,
						u32* returned_stream_no,
						char* returned_file_name,
						u64* returned_file_size,
						u64* returned_file_position,
						u64* returned_time_current)
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_OPEN);

		// Add the parameters in descending order of size.
		if (file_name)
		{
			packet.AddParameter(SFSPacket::PARAMETER_FILE_NAME, file_name);
		}
		if (time_current)
		{
			packet.AddParameter(SFSPacket::PARAMETER_TIME_CURRENT, &time_current);
		}
		packet.AddParameter(SFSPacket::PARAMETER_CHANNEL, &channel);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STREAM_NO)
			{
				*returned_stream_no = ntohl(*((u32*)value));
				continue;
			}
			if (returned_file_name && (id == SFSPacket::PARAMETER_FILE_NAME))
			{
				memcpy(returned_file_name, (const char*)value, 64);
				continue;
			}
			if (returned_file_size && (id == SFSPacket::PARAMETER_FILE_SIZE))
			{
				*returned_file_size = ntohll(*((u64*)value));
				continue;
			}
			if (returned_file_position && (id == SFSPacket::PARAMETER_FILE_POSITION))
			{
				*returned_file_position = ntohll(*((u64*)value));
				continue;
			}
			if (returned_time_current && (id == SFSPacket::PARAMETER_TIME_CURRENT))
			{
				*returned_time_current = ntohll(*((u64*)value));
				continue;
			}
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
		}
		return status;
	}

	u32 SFSClient::Create(const char* file_name, 
						  u32 channel, 
						  u32* returned_stream_no,
						  u64* returned_time_current)
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_CREATE);

		// Add the parameters in descending order of size.
		packet.AddParameter(SFSPacket::PARAMETER_FILE_NAME, file_name);
		packet.AddParameter(SFSPacket::PARAMETER_CHANNEL, &channel);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STREAM_NO)
			{
				*returned_stream_no = ntohl(*((u32*)value));
				continue;
			}
			if (returned_time_current && (id == SFSPacket::PARAMETER_TIME_CURRENT))
			{
				*returned_time_current = ntohll(*((u64*)value));
				continue;
			}
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
		}
		return status;
	}

	u32 SFSClient::Close(u32 stream_no)
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_CLOSE);

		// Add the parameters in descending order of size.
		packet.AddParameter(SFSPacket::PARAMETER_STREAM_NO, &stream_no);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
		}
		return status;
	}

	u32 SFSClient::Read(u32 stream_no,
						void* buf,
						u64 buffer_size,
						u64 data_length,
						u64* returned_data_length,
						u64* returned_bytes_remaining)
	{
		// buf specifies where the data is to be read to
		// buffer_size specifies the maximum number of bytes that can be read
		// if (data_length != 0) then data_length specifies the number of bytes to read
		// if (data_length == 0) then the remainder of the current data chunk will be read
		// *returned_data_length is set to the total number of bytes read
		// *returned_data_remaining is set to 0 if (data_length != 0), otherwise the number of bytes at the end of the chunk that couldn't be read
		if (buffer_size < data_length)
		{
			return STATUS_BUFFER_TOO_SMALL;
		}
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_READ);

		// Add the parameters in descending order of size.
		if (data_length > 0)
		{
			packet.AddParameter(SFSPacket::PARAMETER_DATA_LENGTH, &data_length);
		}
		else if(buffer_size > 0)
		{
			packet.AddParameter(SFSPacket::PARAMETER_BUFFER_SIZE, &buffer_size);
		}
		packet.AddParameter(SFSPacket::PARAMETER_STREAM_NO, &stream_no);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket((char*)buf, buffer_size) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		if ((packet.GetPayloadLength() > buffer_size) && (buffer_size != 0))
		{
			error_code = 0;
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		u64 bytes_remaining = 0;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_BYTES_REMAINING)
			{
				bytes_remaining = ntohll(*((u64*)value));
				continue;
			}
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
		}
		if (status != STATUS_SUCCESS)
		{
			return status;
		}
		if (returned_data_length)
		{
			*returned_data_length = packet.GetPayloadLength();
		}
		if (returned_bytes_remaining)
		{
			*returned_bytes_remaining = bytes_remaining;
		}
		return status;
	}

	u32 SFSClient::Read(u32 stream_no,
						void* buf,
						u64 buffer_size,
						u64 time_start,
						u64 time_end,
						u64* returned_data_length,
						u64* returned_bytes_remaining,
						u64* returned_time_current,
						u64* returned_file_position)
	{
		// buf specifies where the data is to be read to
		// buffer_size specifies the maximum number of bytes that can be read
		// *returned_data_length is set to the total number of bytes read
		// *returned_data_remaining is set to the number of bytes remaining in the time range that couldn't be read

		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_BULK_READ);

		// Add the parameters in descending order of size.
		if(buffer_size > 0)
		{
		    packet.AddParameter(SFSPacket::PARAMETER_BUFFER_SIZE, &buffer_size);
		}
		packet.AddParameter(SFSPacket::PARAMETER_TIME_START, &time_start);
		packet.AddParameter(SFSPacket::PARAMETER_TIME_END, &time_end);
		packet.AddParameter(SFSPacket::PARAMETER_STREAM_NO, &stream_no);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket((char*)buf, buffer_size) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		if ((packet.GetPayloadLength() > buffer_size) && (buffer_size !=0))
		{
			error_code = 0;
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		u64 bytes_remaining = 0;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_BYTES_REMAINING)
			{
				bytes_remaining = ntohll(*((u64*)value));
				continue;
			}
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
			if (returned_time_current && (id == SFSPacket::PARAMETER_TIME_CURRENT))
			{
				*returned_time_current = ntohll(*((u64*)value));
				continue;
			}
			if (returned_file_position && (id == SFSPacket::PARAMETER_FILE_POSITION))
			{
				*returned_file_position = ntohll(*((u64*)value));
				continue;
			}
		}
		if (status != STATUS_SUCCESS)
		{
			return status;
		}
		if (returned_data_length)
		{
			*returned_data_length = packet.GetPayloadLength();
		}
		if (returned_bytes_remaining)
		{
			*returned_bytes_remaining = bytes_remaining;
		}
		return status;
	}

	u32 SFSClient::Write(u32 stream_no,
						 const void* buf,
						 u64 data_length,
						 u32 data_tag,
						 u64* returned_time_current,
						 u64* returned_data_length)
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_WRITE);
		packet.SetPayloadLength(data_length);

		// Add the parameters in descending order of size.
		packet.AddParameter(SFSPacket::PARAMETER_STREAM_NO, &stream_no);
		packet.AddParameter(SFSPacket::PARAMETER_DATA_TAG, &data_tag);

		if (network_session.SendPacket((const char*)buf) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
			if (returned_time_current && (id == SFSPacket::PARAMETER_TIME_CURRENT))
			{
				*returned_time_current = ntohll(*((u64*)value));
				continue;
			}
			if (returned_data_length && (id == SFSPacket::PARAMETER_DATA_LENGTH))
			{
				*returned_data_length = ntohll(*((u64*)value));
				continue;
			}
		}
		return status;
	}

	u32 SFSClient::Tell(u32 stream_no,
						u64* returned_time_current,
						u64* returned_file_position,
						u64* returned_file_size,
						u64* returned_time_start,
						u64* returned_time_end)
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_TELL);

		// Add the parameters in descending order of size.
		packet.AddParameter(SFSPacket::PARAMETER_STREAM_NO, &stream_no);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
			if (returned_time_current && (id == SFSPacket::PARAMETER_TIME_CURRENT))
			{
				*returned_time_current = ntohll(*((u64*)value));
				continue;
			}
			if (returned_time_start && (id == SFSPacket::PARAMETER_TIME_START))
			{
				*returned_time_start = ntohll(*((u64*)value));
				continue;
			}
			if (returned_time_end && (id == SFSPacket::PARAMETER_TIME_END))
			{
				*returned_time_end = ntohll(*((u64*)value));
				continue;
			}
			if (returned_file_position && (id == SFSPacket::PARAMETER_FILE_POSITION))
			{
				*returned_file_position = ntohll(*((u64*)value));
				continue;
			}
			if (returned_file_size && (id == SFSPacket::PARAMETER_FILE_SIZE))
			{
				*returned_file_size = ntohll(*((u64*)value));
				continue;
			}
		}
		return status;
	}

	u32 SFSClient::Seek(u32 stream_no,
						s64 file_offset,
						u8 seek_whence,
						u64 time_start,
						u32 data_tag,
						u8 seek_direction,
						u64* returned_time_current,
						u64* returned_file_position,
						u32* returned_data_tag)
	{
		// if (file_offset != -1) then file_offset and seek_whence are valid and time_start is ignored
		// if (file_offset == -1) then file_offset and seek_whence are ignored and time_start is valid (if it's non-zero)
		// if (seek_direction != 0xff) then data_tag and seek_direction are valid
		// if (seek_direction == 0xff) then data_tag and seek_direction are ignored
		// See the protocol document for the implications of this.
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_SEEK);

		// Add the parameters in descending order of size.
		if (seek_whence != 0xff)
		{
			packet.AddParameter(SFSPacket::PARAMETER_FILE_OFFSET, &file_offset);
		}
		if ((seek_whence == 0xff) && (time_start != 0))
		{
			packet.AddParameter(SFSPacket::PARAMETER_TIME_START, &time_start);
		}
		if (seek_direction != 0xff)
		{
			packet.AddParameter(SFSPacket::PARAMETER_DATA_TAG, &data_tag);
		}
		packet.AddParameter(SFSPacket::PARAMETER_STREAM_NO, &stream_no);
		if (seek_direction != 0xff)
		{
			packet.AddParameter(SFSPacket::PARAMETER_SEEK_DIRECTION, &seek_direction);
		}
		if ((u64)file_offset != (u64)-1)
		{
			packet.AddParameter(SFSPacket::PARAMETER_SEEK_WHENCE, &seek_whence);
		}

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
			if (returned_time_current && (id == SFSPacket::PARAMETER_TIME_CURRENT))
			{
				*returned_time_current = ntohll(*((u64*)value));
				continue;
			}
			if (returned_file_position && (id == SFSPacket::PARAMETER_FILE_POSITION))
			{
				*returned_file_position = ntohll(*((u64*)value));
				continue;
			}
			if (returned_data_tag && (id == SFSPacket::PARAMETER_DATA_TAG))
			{
				*returned_data_tag = ntohl(*((u32*)value));
				continue;
			}
		}
		return status;
	}

	u32 SFSClient::EraseAll(u64 disk_size)
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_ERASEALL);

		// Add the parameters in descending order of size.
		if (disk_size != (u64)-1)
		{
			packet.AddParameter(SFSPacket::PARAMETER_DISK_SIZE, &disk_size);
		}

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(0, 0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
		}
		return status;
	}
		
	u32 SFSClient::FileQuery(bool distinct,
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
							 u32* returned_result_count)
	{
		SFSPacket& packet = network_session.GetPacket();
		packet.Clear();
		packet.SetCommand(SFSPacket::SFS_CMD_FILE_QUERY);

		// Add the parameters in descending order of size.
		if (file_name)
		{
			packet.AddParameter(SFSPacket::PARAMETER_FILE_NAME, file_name);
		}
		if (limit)
		{
			u64 file_count = limit;
			packet.AddParameter(SFSPacket::PARAMETER_FILE_COUNT, &file_count);
		}
		if (time_start)
		{
			packet.AddParameter(SFSPacket::PARAMETER_TIME_START, &time_start);
		}
		if (use_channel)
		{
			packet.AddParameter(SFSPacket::PARAMETER_CHANNEL, &channel);
		}
		if (string_start)
		{
			packet.AddParameter(SFSPacket::PARAMETER_STRING_START, &string_start);
		}
		if (string_length)
		{
			packet.AddParameter(SFSPacket::PARAMETER_STRING_LENGTH, &string_length);
		}
		u32 file_query_flags = SFSPacket::FQF_FILENAME_USE_WILDCARDS |
							   (distinct ? SFSPacket::FQF_DISTINCT : 0) |
							   (order_descending ? SFSPacket::FQF_ORDER_DESCENDING : 0) |
							   (return_channel ? SFSPacket::FQF_RETURN_CHANNEL : 0) |
							   (return_filename ? SFSPacket::FQF_RETURN_FILENAME : 0) |
							   (return_first_ms ? SFSPacket::FQF_RETURN_FIRST_MS : 0);
		packet.AddParameter(SFSPacket::PARAMETER_FILE_QUERY_FLAGS, &file_query_flags);

		if (network_session.SendPacket(0) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		if (network_session.ReceivePacket(results, 0x100000) == -1)
		{
			error_code = network_session.GetErrorCode();
			Disconnect();
			return STATUS_NETWORK_ERROR;
		}
		*returned_results_size = (u32)packet.GetPayloadLength();

		packet.ResetParameterIterator();
		int id;
		void* value;
		u32 status = STATUS_FAIL;
		while (packet.GetNextParameter(&id, &value) != -1)
		{
			if (id == SFSPacket::PARAMETER_STATUS)
			{
				status = ntohl(*((u32*)value));
				continue;
			}
			else if (id == SFSPacket::PARAMETER_FILE_COUNT)
			{
				u64 file_count = ntohll(*((u64*)value));
				*returned_result_count = (u32)file_count;
			}
		}
		return status;
	}

	int SFSClient::GetNetworkErrorCode()
	{
		return network_session.GetErrorCode();
	}

#pragma endregion SFSClient
};
