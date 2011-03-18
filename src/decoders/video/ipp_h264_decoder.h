#ifndef ipp_decoder_h149
#define ipp_decoder_h149

#ifdef _WIN32
#include <windows.h>
#include "abstractdecoder.h"

class IPPH264Decoder : public CLAbstractVideoDecoder
{
public:
	IPPH264Decoder()
	{
		m_id = dll.createDecoder(0);
	}
	~IPPH264Decoder()
	{
		dll.destroyDecoder(m_id);
	}

	bool decode(CLVideoData& params)
	{
		return dll.decode(m_id, &params);
	}

	void showMotion(bool show ){};

	virtual void setLightCpuMode(bool val) {};

private:

	class Dll
	{
	public:
		Dll();
		bool  init();
		~Dll();

		//======================
		typedef unsigned long  (__stdcall *dll_createDecoder)(int);
		typedef void  (__stdcall *dll_destroyDecoder)(unsigned long);
		typedef int  (__stdcall *dll_decode)(unsigned long id, CLVideoData* params);

		dll_createDecoder createDecoder;
		dll_destroyDecoder destroyDecoder;
		dll_decode decode;

		private:

		HINSTANCE m_dll;
	};

	unsigned long m_id;

public:
	static Dll dll;

};

#endif
#endif //ipp_decoder_h149
