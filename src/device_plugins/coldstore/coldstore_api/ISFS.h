////////////////////////////////////////////////////////////////////////////////
//
// ISFS.h
//
// File version: 1.4
// SDK  version: 1.1.0
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __VERACITY_ISFS_H
#define __VERACITY_ISFS_H

namespace Veracity
{
	// The following may need to be changed, depending on the compiler being used.
    typedef				qint8		s8;
    typedef				qint16		s16;
    typedef				qint32		s32;
    typedef				qint64		s64;
    typedef	            quint8		u8;
    typedef	            quint16		u16;
    typedef	            quint32		u32;
    typedef	            quint64		u64;

	class ISFS
	{
	public:
		enum { VERSION = 3 };
		enum { DEFAULT_PORT = 1042 };
		enum
		{
			STATUS_SUCCESS = 0,
			STATUS_FAIL,
			STATUS_DISK_POWERING_UP,
			STATUS_NO_SPARE_STREAMS,
			STATUS_FILE_NOT_FOUND,
			STATUS_NETWORK_ERROR,
			STATUS_BUFFER_TOO_SMALL,
			STATUS_INVALID_STREAM,
			STATUS_FILE_CLOSED
		};

		// Establish connection to COLDSTORE server.
		virtual u32	Connect(const char* server,
							u16 port = DEFAULT_PORT) = 0;

		// Disconnect from COLDSTORE server.
		virtual u32	Disconnect() = 0;

		// Ping the COLDSTORE.
		virtual u32 Echo() = 0;

		// Open a file for reading.
		virtual u32	Open(const char* file_name,
						 u64 time_current,
						 u32 channel,
						 u32* returned_stream_no,
						 char* returned_file_name = 0,
						 u64* returned_file_size = 0,
						 u64* returned_file_position = 0,
						 u64* returned_time_current = 0) = 0;

		// Create a file for writing.
		virtual u32 Create(const char* file_name, 
						   u32 channel, 
						   u32* returned_stream_no,
						   u64* returned_time_current = 0) = 0;

		// Close a file which is currently open for reading or writing.
		virtual u32 Close(u32 stream_no) = 0;

		// Read a specified number of bytes from a file.
		virtual u32 Read(u32 stream_no,
						 void* buf,
						 u64 buffer_size,
						 u64 data_length,
						 u64* returned_data_length = 0,
						 u64* returned_data_remaining = 0) = 0;

		// Read a specified time range from a file.
		virtual u32 Read(u32 stream_no,
						 void* buf,
						 u64 buffer_size,
						 u64 time_start,
						 u64 time_end,
						 u64* returned_data_length = 0,
						 u64* returned_bytes_remaining = 0,
						 u64* returned_time_current = 0,
						 u64* returned_file_position = 0) = 0;

		// Write a specified number of bytes to a file.
		virtual u32 Write(u32 stream_no,
						  const void* buf,
						  u64 data_length,
						  u32 data_tag,
						  u64* returned_time_current = 0,
						  u64* returned_data_length = 0) = 0;

		// Retrieve information about a file.
		virtual u32 Tell(u32 stream_no,
						 u64* returned_time_current,
						 u64* returned_file_position,
						 u64* returned_file_size,
						 u64* returned_time_start = 0,
						 u64* returned_time_end = 0) = 0;

		// Seek to a position in a file.
		virtual u32 Seek(u32 stream_no,
						 s64 file_offset,
						 u8 seek_whence,
						 u64 time_start,
						 u32 data_tag,
						 u8 seek_direction,
						 u64* returned_time_current = 0,
						 u64* returned_file_position = 0,
						 u32* returned_data_tag = 0) = 0;

		// Retrieve filtered information about all files.
		virtual u32 FileQuery(bool distinct,
							  bool return_channel,
							  bool return_filename,
							  bool return_first_ms,
							  u32 string_start,
							  u32 string_length,
							  char* file_name,
							  u32 channel,
							  bool use_channel,
							  u32 limit,
							  bool order_descending,
							  u64 time_start,
							  char* results,
							  u32* returned_results_size,
							  u32* returned_result_count) = 0;

		virtual u32 EraseAll(u64 disk_size = (u64)-1) = 0;

		virtual int GetNetworkErrorCode() = 0;

		virtual ~ISFS() {}
	};
};

#endif
