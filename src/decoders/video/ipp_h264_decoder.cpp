#include "ipp_h264_decoder.h"

#ifdef Q_OS_WIN

#include <QtCore/QLibrary>

typedef unsigned long (__stdcall *fn_createDecoder)(int);
static fn_createDecoder ptr_createDecoder = 0;
typedef void (__stdcall *fn_destroyDecoder)(unsigned long);
static fn_destroyDecoder ptr_destroyDecoder = 0;
typedef int (__stdcall *fn_decode)(unsigned long id, const CLCompressedVideoData *data, CLVideoDecoderOutput* outFrame);
static fn_decode ptr_decode = 0;

static inline bool resolveIPPDecoder()
{
    static bool resolved = false;
    if (!resolved)
    {
        QLibrary library(QLatin1String("ippdecoder"));
        if (library.load())
        {
            ptr_createDecoder = (fn_createDecoder)library.resolve("createDecoder");
            ptr_destroyDecoder = (fn_destroyDecoder)library.resolve("destroyDecoder");
            ptr_decode = (fn_decode)library.resolve("decode");
        }
        resolved = true;
    }

    return ptr_createDecoder && ptr_destroyDecoder && ptr_decode;
}


IPPH264Decoder::IPPH264Decoder() : CLAbstractVideoDecoder()
{
    m_decoder = resolveIPPDecoder() ? ptr_createDecoder(0) : 0;
}

IPPH264Decoder::~IPPH264Decoder()
{
    if (m_decoder)
        ptr_destroyDecoder(m_decoder);
}

bool IPPH264Decoder::decode(const CLCompressedVideoData& data, CLVideoDecoderOutput* outFrame)
{
    return m_decoder && ptr_decode(m_decoder, &data, outFrame);
}

#endif
