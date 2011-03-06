#include "ipp_h264_decoder.h"

IPPH264Decoder::Dll IPPH264Decoder::dll;

IPPH264Decoder::Dll::Dll()
{

}

bool  IPPH264Decoder::Dll::init()
{

	m_dll  = ::LoadLibrary(L"ippdecoder.dll");

	if(!m_dll)
		return false;

	createDecoder = reinterpret_cast<dll_createDecoder>(::GetProcAddress(m_dll, "createDecoder"));
	if(!createDecoder)
		return false;

	destroyDecoder  = reinterpret_cast<dll_destroyDecoder>(::GetProcAddress(m_dll, "destroyDecoder"));
	if(!destroyDecoder)
		return false;

	decode = reinterpret_cast<dll_decode>(::GetProcAddress(m_dll, "decode"));
	if(!decode)
		return false;

	return true;

}

IPPH264Decoder::Dll::~Dll()
{
	::FreeLibrary(m_dll);
}
