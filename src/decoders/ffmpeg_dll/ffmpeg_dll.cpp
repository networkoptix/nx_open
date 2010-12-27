#include "ffmpeg_dll.h"
#include "base\log.h"
#include <QDir>


// functions: avcodec_find_decoder, avcodec_find_decoder, avcodec_alloc_context, avcodec_open, avcodec_close are not thread safe
// so if client use this class the way that it creates and destroy codecs in different we need to make it safe
QMutex global_ffmpeg_mutex;
FFMPEGCodecDll global_ffmpeg_dll;

void decoderLogCallback(void* pParam, int i, const char* szFmt, va_list args)
{
	//USES_CONVERSION;

	//Ignore debug and info (i == 2 || i == 1) messages
	if(AV_LOG_ERROR != i)
	{
		//return;
	}

	AVCodecContext* pCtxt = (AVCodecContext*)pParam;



	char szMsg[1024];
	vsprintf(szMsg, szFmt, args);
	//if(szMsg[strlen(szMsg)] == '\n')
	{	
		szMsg[strlen(szMsg)-1] = 0;
	}

	cl_log.log("FFMPEG ", szMsg, cl_logERROR);
}

FFMPEGCodecDll::FFMPEGCodecDll()
{

}
bool FFMPEGCodecDll::init()
{

	QDir::setCurrent("./old_ffmpeg");

	//m_dll  = ::LoadLibrary(L"avcodec-52.dll");
	m_dll  = ::LoadLibrary(L"avcodec-51.dll");

	if(!m_dll)
		return false;

	avcodec_init = reinterpret_cast<dll_avcodec_init>(::GetProcAddress(m_dll, "avcodec_init"));
	if(!avcodec_init)
		return false;

	avcodec_find_decoder = reinterpret_cast<dll_avcodec_find_decoder>(::GetProcAddress(m_dll, "avcodec_find_decoder"));
	if(!avcodec_find_decoder)
		return false;

	avcodec_register_all = reinterpret_cast<dll_avcodec_register_all>(::GetProcAddress(m_dll, "avcodec_register_all"));
	if (!avcodec_register_all)
		return false;


	avcodec_alloc_context  = reinterpret_cast<dll_avcodec_alloc_context>(::GetProcAddress(m_dll, "avcodec_alloc_context"));
	if (!avcodec_register_all)
		return false;

	avcodec_alloc_frame  = reinterpret_cast<dll_avcodec_alloc_frame>(::GetProcAddress(m_dll, "avcodec_alloc_frame"));
	if (!avcodec_alloc_frame)
		return false;


	avcodec_open  = reinterpret_cast<dll_avcodec_open>(::GetProcAddress(m_dll, "avcodec_open"));
	if (!avcodec_open)
		return false;


	avcodec_close  = reinterpret_cast<dll_avcodec_close>(::GetProcAddress(m_dll, "avcodec_close"));
	if (!avcodec_close)
		return false;

	ff_print_debug_info = reinterpret_cast<dll_ff_print_debug_info>(::GetProcAddress(m_dll, "ff_print_debug_info"));
	if (!ff_print_debug_info)
		return false;
	
	//m_dll2 = ::LoadLibrary(L"avutil-50.dll");
	m_dll2 = ::LoadLibrary(L"avutil-49.dll");
	if(!m_dll2)
		return false;


	av_free = reinterpret_cast<dll_av_free>(::GetProcAddress(m_dll2, "av_free"));
	if (!av_free)
		return false;

	av_log_set_callback = reinterpret_cast<dll_av_log_set_callback>(::GetProcAddress(m_dll2, "av_log_set_callback"));
	if (!av_log_set_callback)
		return false;
	else
	{
		//av_log_set_callback(decoderLogCallback);
	}



	avcodec_decode_video = reinterpret_cast<dll_avcodec_decode_video>(::GetProcAddress(m_dll, "avcodec_decode_video"));
	if (!avcodec_decode_video)
		return false;
	//==================================================================================

	QDir::setCurrent("../");


	return true;

}
FFMPEGCodecDll::~FFMPEGCodecDll()
{
	::FreeLibrary(m_dll);
	::FreeLibrary(m_dll2);
}

//================================================
