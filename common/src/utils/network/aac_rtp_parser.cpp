#include "aac_rtp_parser.h"
#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "utils/common/synctime.h"
#include "core/datapacket/mediadatapacket.h"

QnAacRtpParser::QnAacRtpParser():
        QnRtpStreamParser()
{
    m_sizeLength = 0;
    m_constantSize = 0;
    m_indexLength = 0;
    m_indexDeltaLength = 0;
    m_CTSDeltaLength = 0;
    m_DTSDeltaLength = 0;
    m_randomAccessIndication = 0;
    m_streamStateIndication = 0;
    m_profile = 0;
    m_bitrate = 0;
    m_frequency = 0;
    m_channels = 0;
    m_streamtype = 0;
    m_auHeaderExists = false;
}

QnAacRtpParser::~QnAacRtpParser()
{
}

void QnAacRtpParser::processIntParam(const QByteArray& param, int& setValue, const QByteArray& checkName)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName == checkName)
        setValue = paramValue.toInt();
}

void QnAacRtpParser::processHexParam(const QByteArray& param, QByteArray& setValue, const QByteArray& checkName)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName == checkName)
        setValue = QByteArray::fromHex(paramValue);
}

void QnAacRtpParser::processStringParam(const QByteArray& param, QByteArray& setValue, const QByteArray& checkName)
{
    int valuePos = param.indexOf('=');
    if (valuePos == -1)
        return;
    QByteArray paramName = param.left(valuePos);
    QByteArray paramValue = param.mid(valuePos+1);
    if (paramName == checkName)
        setValue = paramValue;
}

void QnAacRtpParser::setSDPInfo(QList<QByteArray> lines)
{
    // determine here:
    // 1. sizeLength(au size in bits)  or constantSize

    for (int i = 0; i < lines.size(); ++ i)
    {
        if (lines[i].startsWith("a=rtpmap"))
        {
            QList<QByteArray> params = lines[i].mid(lines[i].indexOf(' ')+1).split('/');
            if (params.size() > 1)
                m_frequency = params[1].trimmed().toInt();
            if (params.size() > 2)
                m_channels = params[2].trimmed().toInt();
        }
        else if (lines[i].startsWith("a=fmtp"))
        {
            QList<QByteArray> params = lines[i].mid(lines[i].indexOf(' ')+1).split(';');
            foreach(QByteArray param, params) 
            {
                param = param.trimmed();
                processIntParam("sizeLength", m_sizeLength, param);
                processIntParam("indexLength", m_indexLength, param);
                processIntParam("indexDeltaLength", m_indexDeltaLength, param);
                processIntParam("CTSDeltaLength", m_CTSDeltaLength, param);
                processIntParam("DTSDeltaLength", m_DTSDeltaLength, param);
                processIntParam("randomAccessIndication", m_randomAccessIndication, param);
                processIntParam("streamStateIndication", m_streamStateIndication, param);
                processIntParam("profile", m_profile, param);
                processIntParam("bitrate", m_bitrate, param);
                processIntParam("streamtype", m_streamtype, param);
                processHexParam("config", m_config, param);
                processStringParam("mode", m_mode, param);
            }
        }   
    }
    m_auHeaderExists = m_sizeLength || m_indexLength || m_indexDeltaLength || m_CTSDeltaLength || m_DTSDeltaLength || m_randomAccessIndication || m_streamStateIndication;

    // todo: build m_adtsHeader here
}

bool QnAacRtpParser::processData(quint8* rtpBuffer, int bufferSize, QList<QnAbstractMediaDataPtr>& result)
{
    result.clear();
    QVector<int> auSize;
    QVector<int> auIndexDelta;
    QVector<int> auCtsDelta;
    QVector<int> auDtsDelta;
    int auIndex = 0;
    bool rapFlag = false;
    int streamStateValue = 0;

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    quint8* end = rtpBuffer + bufferSize;

    bool isLastPacket = rtpHeader->marker;
    try 
    {
        if (m_auHeaderExists)
        {
            int firstHeader = 1;
            if (end - curPtr < 2)
                return false;
            int auHeaderLen = (curPtr[0] << 8) + curPtr[1];
            do {
                BitStreamReader reader(curPtr, curPtr + auHeaderLen);
                int headerLen = m_sizeLength + m_indexLength*firstHeader + m_CTSDeltaLength + m_DTSDeltaLength + m_randomAccessIndication + m_streamStateIndication;
                int bitsLeft = auHeaderLen*8 - reader.getBitsCount();
                if (bitsLeft < headerLen)
                    break;

                if (m_sizeLength)
                    auSize << reader.getBits(m_sizeLength);
                if (m_indexLength) {
                    if (firstHeader)
                        auIndex = reader.getBits(m_indexLength);
                    auIndexDelta << reader.getBits(m_indexDeltaLength);
                }
                if (m_CTSDeltaLength) {
                    if (reader.getBit())
                        auCtsDelta << reader.getBits(m_CTSDeltaLength);
                    else
                        auCtsDelta << 0;
                }
                if (m_DTSDeltaLength) {
                    if (reader.getBit())
                        auDtsDelta << reader.getBits(m_DTSDeltaLength);
                    else
                        auDtsDelta << 0;
                }
                if (m_randomAccessIndication)
                    rapFlag = reader.getBit();
                if (m_streamStateIndication)
                    streamStateValue = reader.getBits(m_streamStateIndication);

                firstHeader = 0;
            } while (1);
            curPtr += auHeaderLen;
        }
    } catch(...) {
        return false;
    }

    for (int i = 0; curPtr < end; ++i)
    {
        int unitSize = m_constantSize;
        if (curPtr + unitSize > end)
            return false;
        
        if (!m_constantSize) {
            if (auSize.size() < i+1)
                return false;
            unitSize = auSize[i];
        }
        QnCompressedAudioDataPtr audioData = QnCompressedAudioDataPtr(new QnCompressedAudioData(CL_MEDIA_ALIGNMENT, unitSize));
        audioData->compressionType = CODEC_ID_AAC;
        audioData->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;

        audioData->data.write(m_adtsHeader);
        audioData->data.write((const char*)curPtr, unitSize);

        curPtr += unitSize;
    }

    return true;
}
