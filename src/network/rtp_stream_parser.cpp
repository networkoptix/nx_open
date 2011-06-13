#include "rtp_stream_parser.h"

CLRtpStreamParser::CLRtpStreamParser(QIODevice* input):
            m_input(input)
{
}

CLRtpStreamParser::~CLRtpStreamParser()
{
    delete m_input;
}

void CLRtpStreamParser::setSDPInfo(const QByteArray& data)
{
    m_sdp = data;
}
