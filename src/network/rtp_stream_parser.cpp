#include "rtp_stream_parser.h"

CLRtpStreamParser::CLRtpStreamParser(RTPIODevice* input):
            m_input(input)
{
}

CLRtpStreamParser::~CLRtpStreamParser()
{
}

void CLRtpStreamParser::setSDPInfo(const QByteArray& data)
{
    m_sdp = data;
}
