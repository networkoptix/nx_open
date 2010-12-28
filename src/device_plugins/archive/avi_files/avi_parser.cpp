#include "avi_parser.h"

CLAviFFMpegDLL avidll;

CLAviFFMpegDLL::CLAviFFMpegDLL()
{
	m_dll=0;
	m_dll2 = 0;
	m_dll3 = 0;
}

bool CLAviFFMpegDLL::init()
{

	m_dll  = ::LoadLibrary(L"avformat-52.dll");
	if(!m_dll)
		return false;

	avformat_version = reinterpret_cast<dll_avformat_version>(::GetProcAddress(m_dll, "avformat_version"));
	if(!avformat_version)
		return false;


	avformat_alloc_context  = reinterpret_cast<dll_avformat_alloc_context>(::GetProcAddress(m_dll, "avformat_alloc_context"));
	if(!avformat_alloc_context)
		return false;

	av_open_input_file  = reinterpret_cast<dll_av_open_input_file>(::GetProcAddress(m_dll, "av_open_input_file"));
	if(!av_open_input_file)
		return false;

	av_find_stream_info  = reinterpret_cast<dll_av_find_stream_info>(::GetProcAddress(m_dll, "av_find_stream_info"));
	if(!av_find_stream_info)
		return false;


	av_read_frame  = reinterpret_cast<dll_av_read_frame>(::GetProcAddress(m_dll, "av_read_frame"));
	if(!av_read_frame)
		return false;

	av_register_all  = reinterpret_cast<dll_av_register_all>(::GetProcAddress(m_dll, "av_register_all"));
	if(!av_register_all)
		return false;

	avformat_seek_file = reinterpret_cast<dll_avformat_seek_file>(::GetProcAddress(m_dll, "avformat_seek_file"));
	if(!avformat_seek_file)
		return false;


	




	m_dll2 = ::LoadLibrary(L"avcodec-52.dll");
	if(!m_dll2)
		return false;
	
	av_init_packet = reinterpret_cast<dll_av_init_packet>(::GetProcAddress(m_dll2, "av_init_packet"));
	if(!av_init_packet )
		return false;

	av_free_packet  = reinterpret_cast<dll_av_free_packet>(::GetProcAddress(m_dll2, "av_free_packet"));
	if(!av_free_packet )
		return false;


	m_dll3 = ::LoadLibrary(L"avutil-50.dll");
	if(!m_dll3)
		return false;


	av_free = reinterpret_cast<dll_av_free>(::GetProcAddress(m_dll3, "av_free"));
	if (!av_free)
		return false;



	return true;
}

CLAviFFMpegDLL::~CLAviFFMpegDLL()
{
	::FreeLibrary(m_dll);
	::FreeLibrary(m_dll2);

}
