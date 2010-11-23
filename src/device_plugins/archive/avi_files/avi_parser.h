#ifndef claavi_parser_h1916
#define claavi_parser_h1916

#include <tchar.h>
#include <windows.h>

struct AVFormatContext;
struct AVInputFormat;
struct AVFormatParameters;
struct AVPacket;

class CLAviFFMpegDLL
{
public:
	CLAviFFMpegDLL();
	bool  init();
	~CLAviFFMpegDLL();
	//============dll =======

	typedef unsigned (*dll_avformat_version)(void);
	typedef AVFormatContext* (*dll_avformat_alloc_context)(void);
	typedef int  (*dll_av_open_input_file)(AVFormatContext **ic_ptr, const char *filename, AVInputFormat *fmt, int buf_size, AVFormatParameters *ap);
	typedef int  (*dll_av_find_stream_info)(AVFormatContext *ic) ;
	typedef void (*dll_av_free)(void *ptr);
	typedef void (*dll_av_init_packet)(AVPacket*pkt);
	typedef int (*dll_av_read_frame)(AVFormatContext *s, AVPacket *pkt);




	dll_avformat_version avformat_version;
	dll_avformat_alloc_context avformat_alloc_context;
	dll_av_open_input_file av_open_input_file;
	dll_av_find_stream_info av_find_stream_info;
	dll_av_read_frame av_read_frame;
	dll_av_init_packet av_init_packet;
	dll_av_free av_free;
	/**/
private:
	HINSTANCE m_dll, m_dll2, m_dll3;
};

static CLAviFFMpegDLL avidll;


#endif //claavi_parser_h1916