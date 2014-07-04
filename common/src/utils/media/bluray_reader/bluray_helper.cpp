
#include "bluray_helper.h"

extern "C"
{
    #include <libavutil/avutil.h>
}

#include "utils/common/util.h"
#include "utils/common/log.h"


#ifdef _MSC_VER
#    pragma warning(disable: 4189) /* C4189: '?' : local variable is initialized but not referenced. */
#    pragma warning(disable: 4101) /* C4101: '?' : unreferenced local variable. */
#endif

static const int DEFAULT_PCR_PID = 4097;
static const int DEFAULT_PMT_PID = 256;

// --------------------- BlurayVideoStream ----------------------
BlurayVideoStream::BlurayVideoStream():
        m_streamHeight(0),
        m_streamWidth(0),
        m_fps(0.0),
        m_isInterlaced(false)
{
}

// --------------------- BlurayVideoStream ----------------------

BlurayAudioStream::BlurayAudioStream():
    m_freq(0),
    m_altFreq(0),
    m_channels(0)
{
}


// --------------------- BluRayCoarseInfo ----------------------

BluRayCoarseInfo::BluRayCoarseInfo():
    m_coarsePts(0),
    m_fineRefID(0),
    m_pktCnt(0)
{
}

BluRayCoarseInfo::BluRayCoarseInfo(quint32 coarsePts, quint32 fineRefID, quint32 pktCnt):
    m_coarsePts(coarsePts),
    m_fineRefID(fineRefID),
    m_pktCnt(pktCnt)
{
}


// --------------------- PMTStreamInfo ----------------------

PMTStreamInfo::PMTStreamInfo() :
    m_streamType(0),
    m_pid(0),
    m_esInfoLen(0),
    m_pmtPID(-1),
    m_priorityStream(false),
    m_isSecondary(false)
{
}

PMTStreamInfo::PMTStreamInfo(int streamType, int pid, quint8* esInfoData, int esInfoLen,
    AbstractBlurayStream* codecReader, const QString& lang, bool secondary)
{
    m_streamType = streamType;
    m_pid = pid;
    memcpy(m_esInfoData, esInfoData, esInfoLen);
    m_esInfoLen = esInfoLen;
    m_codecReader = codecReader;
    memset(m_lang, 0, sizeof(m_lang));
    memcpy(m_lang, lang.toLatin1(), lang.size() < 3 ? lang.size() : 3);
    m_pmtPID = -1;
    m_priorityStream = false;
    m_isSecondary = secondary;
    //m_calcStatistic = false;
}

// --------------------- CLPIStreamInfo ----------------------

CLPIStreamInfo::CLPIStreamInfo(const PMTStreamInfo& pmtStreamInfo):
    M2TSStreamInfo(pmtStreamInfo)
{
    memset(m_country_code, 0, sizeof(m_country_code));
    memset(m_copyright_holder, 0, sizeof(m_copyright_holder));
    memset(m_recording_year, 0, sizeof(m_recording_year));
    memset(m_recording_number, 0, sizeof(m_recording_number));
}

CLPIStreamInfo::CLPIStreamInfo():
    M2TSStreamInfo()
{
    memset(m_language_code, 0, sizeof(m_language_code));
    memset(m_country_code, 0, sizeof(m_country_code));
    memset(m_copyright_holder, 0, sizeof(m_copyright_holder));
    memset(m_recording_year, 0, sizeof(m_recording_year));
    memset(m_recording_number, 0, sizeof(m_recording_number));
}

void CLPIStreamInfo::readString(char* dest, BitStreamReader& reader, int size)
{
    for (int i = 0; i < size; i++)
        dest[i] = reader.getBits(8);
    dest[size] = 0;
}

void CLPIStreamInfo::writeString(const char* dest, BitStreamWriter& writer, int size)
{
    for (int i = 0; i < size; i++)
        writer.putBits(8, dest[i]);
}

// --------------------- CLPIParser -----------------------------


void CLPIStreamInfo::ISRC(BitStreamReader& reader)
{
    readString(m_country_code, reader, 2);
    readString(m_copyright_holder, reader, 3);
    readString(m_recording_year, reader, 2);
    readString(m_recording_number, reader, 5);
}

void CLPIStreamInfo::composeISRC(BitStreamWriter& writer) const
{
    writeString("\x30\x30", writer, 2);
    writeString("\x30\x30\x30", writer, 3);
    writeString("\x30\x30", writer, 2);
    writeString("\x30\x30\x30\x30\x30", writer, 5);
}

void CLPIStreamInfo::parseStreamCodingInfo(BitStreamReader& reader)
{
    int length = reader.getBits(8);
    Q_UNUSED(length);
    m_stream_coding_type = reader.getBits(8);

    if (m_stream_coding_type==0x02 || m_stream_coding_type==0x1B || m_stream_coding_type==0xEA)
    {
        m_video_format  = reader.getBits(4);
        m_frame_rate_index = reader.getBits(4);
        m_aspect_ratio_index = reader.getBits(4);
        reader.skipBits(2); //reserved_for_future_use 2 bslbf
        bool cc_flag  = reader.getBit();
        Q_UNUSED(cc_flag);
        reader.skipBits(17); // reserved_for_future_use 17 bslbf
        ISRC(reader);
        reader.skipBits(32); // reserved_for_future_use 32 bslbf
    } else if (m_stream_coding_type==0x80 || m_stream_coding_type==0x81 ||
               m_stream_coding_type==0x82 || m_stream_coding_type==0x83 ||
               m_stream_coding_type==0x84 || m_stream_coding_type==0x85 ||
               m_stream_coding_type==0x86 || m_stream_coding_type==0xA1 ||
               m_stream_coding_type==0xA2)
    {
        m_audio_presentation_type  = reader.getBits(4);
        m_sampling_frequency_index  = reader.getBits(4);
        readString(m_language_code, reader, 3);
        ISRC(reader);
        reader.skipBits(32);
    }
    else if (m_stream_coding_type==0x90) {
        // Presentation Graphics stream
        readString(m_language_code, reader, 3);
        reader.skipBits(8); // reserved_for_future_use 8 bslbf
        ISRC(reader);
        reader.skipBits(32); // reserved_for_future_use 32 bslbf
    }
    else if (m_stream_coding_type==0x91)
    {
        // Interactive Graphics stream
        readString(m_language_code, reader, 3);
        reader.skipBits(8); //reserved_for_future_use 8 bslbf
        ISRC(reader);
        reader.skipBits(32); // reserved_for_future_use 32 bslbf
    }
    else if (m_stream_coding_type==0x92)
    {
        // Text subtitle stream
        m_character_code = reader.getBits(8);
        readString(m_language_code, reader, 3);
        ISRC(reader);
        reader.skipBits(32); // reserved_for_future_use 32 bslbf
    }
}

void CLPIStreamInfo::composeStreamCodingInfo(BitStreamWriter& writer) const
{
    quint8* lengthPos = writer.getBuffer() + writer.getBitsCount()/8;
    writer.putBits(8,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, m_stream_coding_type);

    if (m_stream_coding_type==0x02 || m_stream_coding_type==0x1B || m_stream_coding_type==0xEA)
    {
        writer.putBits(4, m_video_format);
        writer.putBits(4, m_frame_rate_index);
        writer.putBits(4, m_aspect_ratio_index);
        writer.putBits(2,0); //reserved_for_future_use 2 bslbf
        writer.putBit(0); // cc_flag
        writer.putBits(17,0); // reserved_for_future_use 17 bslbf
        composeISRC(writer);
        writer.putBits(32, 0); // reserved_for_future_use 32 bslbf
    } else if (m_stream_coding_type==0x80 || m_stream_coding_type==0x81 ||
               m_stream_coding_type==0x82 || m_stream_coding_type==0x83 ||
               m_stream_coding_type==0x84 || m_stream_coding_type==0x85 ||
               m_stream_coding_type==0x86 || m_stream_coding_type==0xA1 ||
               m_stream_coding_type==0xA2) {
        writer.putBits(4, m_audio_presentation_type);
        writer.putBits(4, m_sampling_frequency_index);
        writeString(m_language_code, writer, 3);
        composeISRC(writer);
        writer.putBits(32,0);
    } else if (m_stream_coding_type==0x90)
    {
        // Presentation Graphics stream
        writeString(m_language_code, writer, 3);
        writer.putBits(8,0); // reserved_for_future_use 8 bslbf
        composeISRC(writer);
        writer.putBits(32,0); // reserved_for_future_use 32 bslbf
    } else if (m_stream_coding_type==0x91)
    {
        // Interactive Graphics stream
        writeString(m_language_code, writer, 3);
        writer.putBits(8,0); //reserved_for_future_use 8 bslbf
        composeISRC(writer);
        writer.putBits(32,0); // reserved_for_future_use 32 bslbf
    } else if (m_stream_coding_type==0x92)
    {
        // Text subtitle stream
        writer.putBits(8,m_character_code);
        writeString(m_language_code, writer, 3);
        composeISRC(writer);
        writer.putBits(32,0); // reserved_for_future_use 32 bslbf
    }
    *lengthPos = writer.getBitsCount()/8 - beforeCount;
}

void CLPIParser::parseProgramInfo(quint8* buffer, quint8* end)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    quint32 length = reader.getBits(32);
    Q_UNUSED(length);
    reader.skipBits(8); // reserved
    quint8 number_of_program_sequences = reader.getBits(8);
    for (int i=0; i < number_of_program_sequences; i++)
    {
        m_programInfo.push_back(CLPIProgramInfo());
        m_programInfo[i].m_SPN_program_sequence_start = reader.getBits(32);
        m_programInfo[i].m_program_map_PID = reader.getBits(16);
        m_programInfo[i].m_number_of_streams_in_ps = reader.getBits(8);
        reader.skipBits(8);
        for (int stream_index=0; stream_index < m_programInfo[i].m_number_of_streams_in_ps; stream_index++)
        {
            int pid = reader.getBits(16);
            CLPIStreamInfo streamInfo;
            streamInfo.parseStreamCodingInfo(reader);
            m_streamInfo.insert(pid, streamInfo);
        }
    }
}

void CLPIParser::composeProgramInfo(BitStreamWriter& writer)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0); // reserved
    writer.putBits(8, 1); // number_of_program_sequences
    //for (int i=0; i < number_of_program_sequences; i++)
    {
        //m_programInfo.push_back(CLPIProgramInfo());
        writer.putBits(32, 0); // m_programInfo[i].m_SPN_program_sequence_start
        writer.putBits(16, DEFAULT_PMT_PID); // m_programInfo[i].program_map_PID =
        writer.putBits(8, m_streamInfo.size()); // m_programInfo[i].number_of_streams_in_ps
        writer.putBits(8,0); // reserved_for_future_use 8 bslbf
        //for (int i=0; i < m_streamInfo.size(); i++)
        for (QMap<int, CLPIStreamInfo>::const_iterator itr = m_streamInfo.begin();
             itr != m_streamInfo.end(); ++itr)
        {
            writer.putBits(16, itr.key()); // pid
            itr.value().composeStreamCodingInfo(writer);
        }
    }
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

void CLPIParser::TS_type_info_block(BitStreamReader& reader)
{
    quint16 length = reader.getBits(16);
    Q_UNUSED(length);
    quint8 Validity_flags  = reader.getBits(8); // 1000 0000b is tipical value
    Q_UNUSED(Validity_flags);
    CLPIStreamInfo::readString(m_format_identifier, reader, 4);    // HDMV
    //Network_information 8*9 bslbf zerro
    for (int i = 0; i < 9; i++)
        reader.skipBits(8);
    //Stream_format_name 8*16 bslbf zerro
    for (int i = 0; i < 16; i++)
        reader.skipBits(8);
}

void CLPIParser::composeTS_type_info_block(BitStreamWriter& writer)
{
    //quint16 length = reader.getBits(16);
    quint16* lengthPos = (quint16*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(16,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8,0x80); // Validity_flags
    CLPIStreamInfo::writeString("HDMV", writer, 4);
    //Network_information 8*9 bslbf zerro
    for (int i = 0; i < 9; i++)
        writer.putBits(8,0);
    //Stream_format_name 8*16 bslbf zerro
    for (int i = 0; i < 16; i++)
        writer.putBits(8,0);
    *lengthPos = ntohs(writer.getBitsCount()/8 - beforeCount);
}

void CLPIParser::parseClipInfo(BitStreamReader& reader)
{
    quint32 length = reader.getBits(32);
    Q_UNUSED(length);
    reader.skipBits(16); //reserved_for_future_use 16 bslbf
    m_clip_stream_type = reader.getBits(8); // 1 - AV stream
    m_application_type = reader.getBits(8); // 1 - Main TS for a main-path of Movie
    reader.skipBits(31); //reserved_for_future_use 31 bslbf
    m_is_ATC_delta = reader.getBit(); // 1 bslbf
    m_TS_recording_rate = reader.getBits(32); // kbps in bytes/sec
    m_number_of_source_packets  = reader.getBits(32); //number of TS packets?
    for (int i = 0; i < 32; i++)
        reader.skipBits(32);
    TS_type_info_block(reader);
    /*
    if (m_is_ATC_delta==1b) {
        reserved_for_future_use 8 bslbf
        number_of_ATC_delta_entries 8 uimsbf
        for (i=0; i<number_of_ATC_delta_entries; i++) {
            ATC_delta[i] 32 uimsbf
            following_Clip_Information_file_name[i] 8*5 bslbf
            Clip_codec_identifier 8*4 bslbf
            reserved_for_future_use 8 bslbf
        }
    }
    if (m_application_type==6){
        reserved_for_future_use 8 bslbf
        number_of_font_files 8 uimsbf
        for (font_id=0; font_id<number_of_font_files; font_id++) {
            font_file_name[font_id] 8*5 bslbf
            reserved_for_future_use 8 bslbf
        }
    }
    */
}

void CLPIParser::composeClipInfo(BitStreamWriter& writer)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(16,0); //reserved_for_future_use 16 bslbf
    writer.putBits(8, m_clip_stream_type); // 1 - AV stream
    writer.putBits(8, m_application_type); // 1 - Main TS for a main-path of Movie
    writer.putBits(31,0); //reserved_for_future_use 31 bslbf
    writer.putBit(m_is_ATC_delta); // 1 bslbf
    writer.putBits(32, m_TS_recording_rate); // kbps in bytes/sec
    writer.putBits(32, m_number_of_source_packets); //number of TS packets?
    for (int i = 0; i < 32; i++)
        writer.putBits(32, 0); // reserved
    composeTS_type_info_block(writer);
    if (m_is_ATC_delta)
        THROW_BITSTREAM_ERR2("CLPI m_is_ATC_delta is not implemented now.");
    if (m_application_type==6)
        THROW_BITSTREAM_ERR2("CLPI m_application_type==6 is not implemented now.");
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

void CLPIParser::parseSequenceInfo(quint8* buffer, quint8* end)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    quint32 length = reader.getBits(32);
    Q_UNUSED(length);
    reader.skipBits(8); //reserved_for_word_align 8 bslbf
    quint8 number_of_ATC_sequences  = reader.getBits(8); // 1 is tipical value
    for (int atc_id = 0; atc_id < number_of_ATC_sequences; atc_id++)
    {
        quint32 SPN_ATC_start = reader.getBits(32); // 0 is tipical value
        Q_UNUSED(SPN_ATC_start);
        quint8 number_of_STC_sequences = reader.getBits(8);
        int offset_STC_id = reader.getBits(8);
        for (int stc_id=offset_STC_id; stc_id < number_of_STC_sequences + offset_STC_id; stc_id++)
        {
            int PCR_PID = reader.getBits(16);
            Q_UNUSED(PCR_PID);
            quint32 SPN_STC_start = reader.getBits(32);
            Q_UNUSED(SPN_STC_start);
            m_presentation_start_time = reader.getBits(32);
            m_presentation_end_time = reader.getBits(32);
        }
    }
}

void CLPIParser::composeSequenceInfo(BitStreamWriter& writer)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0); //reserved_for_word_align 8 bslbf
    writer.putBits(8, 1); // number_of_ATC_sequences
    //for (int atc_id = 0; atc_id < number_of_ATC_sequences; atc_id++)
    {
        writer.putBits(32, 0); // SPN_ATC_start
        writer.putBits(8, 1); // number_of_STC_sequences
        writer.putBits(8, 0); // offset_STC_id
        //for (int stc_id=offset_STC_id; stc_id < number_of_STC_sequences + offset_STC_id; stc_id++)
        {
            writer.putBits(16, DEFAULT_PCR_PID);
            writer.putBits(32, 0); // SPN_STC_start
            writer.putBits(32, m_presentation_start_time);
            writer.putBits(32, m_presentation_end_time);
        }
    }
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

void CLPIParser::parseCPI(quint8* buffer, quint8* end) {
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    quint32 length = reader.getBits(32);
    if (length != 0) {
        //reserved_for_word_align 12 bslbf
        reader.skipBits(12);
        int CPI_type = reader.getBits(4); // 1 is tipical value
        CPI_type = CPI_type;
        //EP_map();
    }
}

void CLPIParser::composeCPI(BitStreamWriter& writer)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;
    //if (length != 0)
    {
        //reserved_for_word_align 12 bslbf
        writer.putBits(12,0);
        writer.putBits(4, 1); // CPI_type
        composeEP_map(writer);
    }
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

void CLPIParser::composeEP_map(BitStreamWriter& writer)
{
    quint32 beforeCount = writer.getBitsCount()/8;
    QVector<CLPIStreamInfo> processStream;
    int EP_stream_type = 1; //[k] 4 bslbf
    for (QMap<int, CLPIStreamInfo>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
    {
        int coding_type = itr.value().m_stream_coding_type;
        if (coding_type==0x02 || coding_type==0x1B || coding_type==0xEA)
            processStream.push_back(itr.value());
    }
    if (processStream.size() == 0)
    {
        for (QMap<int, CLPIStreamInfo>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        {
            int coding_type = itr.value().m_stream_coding_type;
            if (coding_type==0x80 || coding_type==0x81 ||
                coding_type==0x82 || coding_type==0x83 ||
                coding_type==0x84 || coding_type==0x85 ||
                coding_type==0x86 || coding_type==0xA1 ||
                coding_type==0xA2)
            {
                processStream.push_back(itr.value());
                EP_stream_type = 3;
                break;
            }
        }
    }
    if (processStream.size() == 0)
        THROW_BITSTREAM_ERR2("Can't create EP map. One audio or video stream is needed.");
    // ------------------
    writer.putBits(8,0); //reserved_for_word_align 8 bslbf
    writer.putBits(8,processStream.size()); //number_of_stream_PID_entries 8 uimsbf
    QVector<quint32*> epStartAddrPos;

    for (int i = 0; i < processStream.size(); i++)
    {

        writer.putBits(16, processStream[i].m_ref_to_stream_PID_of_mainClip);//stream_PID[k] 16 bslbf
        writer.putBits(10, 0); //reserved_for_word_align 10 bslbf
        writer.putBits(4, EP_stream_type);
        QVector<BluRayCoarseInfo> coarseInfo = buildCoarseInfo(processStream[i]);
        writer.putBits(16, coarseInfo.size()); // number_of_EP_coarse_entries[k] 16 uimsbf
        if (processStream[i].m_index.size() > 0)
            writer.putBits(18, processStream[i].m_index[m_clpiNum].size()); //number_of_EP_fine_entries[k] 18 uimsbf
        else
            writer.putBits(18, 0);
        epStartAddrPos.push_back((quint32*) (writer.getBuffer() + writer.getBitsCount()/8));
        writer.putBits(32, 0); //EP_map_for_one_stream_PID_start_address[k] 32 uimsbf
    }
    while (writer.getBitsCount() % 16 != 0)
        writer.putBits(8,0); //padding_word 16 bslbf

    for (int i=0; i < processStream.size(); i++)
    {
        *epStartAddrPos[i] = htonl(writer.getBitsCount()/8 - beforeCount);
        composeEP_map_for_one_stream_PID(writer, processStream[i]);
        while (writer.getBitsCount() % 16 != 0)
            writer.putBits(8,0); //padding_word 16 bslbf
    }

}

QVector<BluRayCoarseInfo> CLPIParser::buildCoarseInfo(M2TSStreamInfo& streamInfo)
{
    QVector<BluRayCoarseInfo> rez;
    if (streamInfo.m_index.size() == 0)
        return rez;
    quint32 cnt = 0;
    quint32 lastPktCnt = 0;
    quint32 lastCoarsePts = 0;
    PMTIndex& curIndex = streamInfo.m_index[m_clpiNum];
    for (PMTIndex::const_iterator itr = curIndex.begin(); itr != curIndex.end();++itr)
    {
        const PMTIndexData& indexData = itr.value();
        quint32 newCoarsePts = itr.key() >> 19;
        quint32 lastCoarseSPN = lastPktCnt & 0xfffe0000;
        quint32 newCoarseSPN = indexData.m_pktCnt & 0xfffe0000;
        if (rez.size() == 0 || newCoarsePts != lastCoarsePts || lastCoarseSPN != newCoarseSPN)
        {
            rez.push_back(BluRayCoarseInfo(newCoarsePts, cnt, indexData.m_pktCnt));
        }
        lastCoarsePts = newCoarsePts;
        lastPktCnt = indexData.m_pktCnt;
        cnt++;
    }
    return rez;
}

void CLPIParser::composeEP_map_for_one_stream_PID(BitStreamWriter& writer, M2TSStreamInfo& streamInfo)
{
    quint32* epFineStartAddr = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    quint32 beforePos = writer.getBitsCount()/8;
    writer.putBits(32,0); //EP_fine_table_start_address 32 uimsbf
    QVector<BluRayCoarseInfo> coarseInfo = buildCoarseInfo(streamInfo);
    for (int i = 0; i < coarseInfo.size(); i++) {
        writer.putBits(18, coarseInfo[i].m_fineRefID); //ref_to_EP_fine_id[i] 18 uimsbf
        writer.putBits(14, coarseInfo[i].m_coarsePts);  //PTS_EP_coarse[i] 14 uimsbf
        writer.putBits(32, coarseInfo[i].m_pktCnt);   //SPN_EP_coarse[i] 32 uimsbf
    }
    while (writer.getBitsCount() % 16 != 0)
        writer.putBits(8,0); //padding_word 16 bslbf
    *epFineStartAddr = htonl(writer.getBitsCount()/8 - beforePos);
    if (streamInfo.m_index.size() > 0)
    {
        const PMTIndex& curIndex = streamInfo.m_index[m_clpiNum];
        for (PMTIndex::const_iterator itr = curIndex.begin(); itr != curIndex.end();++itr)
        {
            const PMTIndexData& indexData = itr.value();
            writer.putBit(0); //is_angle_change_point[EP_fine_id] 1 bslbf
            int endCode = 0;
            if (indexData.m_frameLen > 0) {
                if (indexData.m_frameLen < 131072)
                    endCode = 1;
                else if (indexData.m_frameLen < 262144)
                    endCode = 2;
                else if (indexData.m_frameLen < 393216)
                    endCode = 3;
                else if (indexData.m_frameLen < 589824)
                    endCode = 4;
                else if (indexData.m_frameLen < 917504)
                    endCode = 5;
                else if (indexData.m_frameLen < 1310720)
                    endCode = 6;
                else
                    endCode = 7;
            }
            writer.putBits(3, endCode); //I_end_position_offset[EP_fine_id] 3 bslbf
            writer.putBits(11, (itr.key() >> 9) % 2048);  //PTS_EP_fine[EP_fine_id] 11 uimsbf
            writer.putBits(17, indexData.m_pktCnt % (65536*2)); //SPN_EP_fine[EP_fine_id] 17 uimsbf
        }
    }
}

void CLPIParser::parseClipMark(quint8* buffer, quint8* end) {
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    quint32 length = reader.getBits(32);
    Q_UNUSED(length);
}

void CLPIParser::composeClipMark(BitStreamWriter& writer)
{
    writer.putBits(32, 0);
}

bool CLPIParser::parse(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    quint64 fileSize = file.size();
    quint8* buffer = new quint8[fileSize];
    if (!file.read((char*)buffer, fileSize)) {
        delete [] buffer;
        return false;
    }
    try {
        parse(buffer, fileSize);
        delete [] buffer;
        return true;
    } catch(...) {
        delete [] buffer;
        return false;
    }
}

void CLPIParser::parseExtensionData(quint8* /*buffer*/, quint8* /*end*/)
{
}

void CLPIParser::parse(quint8* buffer, int len)
{
    BitStreamReader reader;
    try {
        reader.setBuffer(buffer, buffer + len);

        CLPIStreamInfo::readString(m_type_indicator, reader, 4);
        CLPIStreamInfo::readString(version_number, reader, 4);
        quint32 sequenceInfo_start_address = reader.getBits(32);
        quint32 programInfo_start_address = reader.getBits(32);
        quint32 CPI_start_address = reader.getBits(32);
        quint32 clipMark_start_address = reader.getBits(32);
        quint32 extensionData_start_address = reader.getBits(32);
        for (int i = 0; i < 3; i++)
            reader.skipBits(32); //reserved_for_future_use 96 bslbf
        parseClipInfo(reader);
        //for (int i=0; i<N1; i++) padding_word 16 bslbf
        parseSequenceInfo(buffer + sequenceInfo_start_address, buffer + len);
        //for (i=0; i<N2; i++) { padding_word 16 bslbf
        parseProgramInfo(buffer + programInfo_start_address, buffer + len);
        //for (i=0; i<N3; i++) padding_word 16 bslbf
        parseCPI(buffer + CPI_start_address, buffer + len);
        //for (i=0; i<N4; i++) padding_word 16 bslbf
        parseClipMark(buffer + clipMark_start_address, buffer + len);
        //for (i=0; i<N5; i++) padding_word 16 bslbf
        parseExtensionData(buffer + extensionData_start_address, buffer + len);
        //for (i=0; i<N6; i++) padding_word 16 bslbf
    } catch (BitStreamException& e)
    {
        THROW_BITSTREAM_ERR2("Can't parse clip info file: unexpected end of data");
    }
}

int CLPIParser::compose(quint8* buffer, int bufferSize)
{
    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + bufferSize);
    CLPIStreamInfo::writeString("HDMV", writer, 4);
    CLPIStreamInfo::writeString(version_number, writer, 4);
    quint32* sequenceInfo_pos = (quint32*) (buffer + writer.getBitsCount()/8);
    writer.putBits(32, 0); // sequenceInfo_start_address
    quint32* programInfo_pos = (quint32*) (buffer + writer.getBitsCount()/8);
    writer.putBits(32, 0); // programInfo_start_address
    quint32* CPI_pos = (quint32*) (buffer + writer.getBitsCount()/8);
    writer.putBits(32, 0); // CPI start address
    quint32* clipMark_pos = (quint32*) (buffer + writer.getBitsCount()/8);
    writer.putBits(32, 0); // clipMark
    writer.putBits(32,0); // do not use extension data
    for (int i = 0; i < 3; i++)
        writer.putBits(32,0); //reserved_for_future_use 96 bslbf

    composeClipInfo(writer);
    while (writer.getBitsCount()%16 != 0) writer.putBits(8,0);
    *sequenceInfo_pos = htonl(writer.getBitsCount()/8);
    composeSequenceInfo(writer);
    while (writer.getBitsCount()%16 != 0) writer.putBits(8,0);
    *programInfo_pos = htonl(writer.getBitsCount()/8);
    composeProgramInfo(writer);
    while (writer.getBitsCount()%16 != 0) writer.putBits(8,0);
    *CPI_pos = htonl(writer.getBitsCount()/8);
    composeCPI(writer);
    while (writer.getBitsCount()%16 != 0) writer.putBits(8,0);
    *clipMark_pos = htonl(writer.getBitsCount()/8);
    composeClipMark(writer);
    while (writer.getBitsCount()%16 != 0) writer.putBits(8,0);
    writer.flushBits();
    return writer.getBitsCount() / 8;
}

// -------------------------- MPLSParser ----------------------------

bool MPLSParser::parse(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    quint64 fileSize = file.size();
    quint8* buffer = new quint8[fileSize];
    if (!file.read( (char*) buffer, fileSize))
    {
        delete [] buffer;
        return false;
    }
    try {
        parse(buffer, fileSize);
        delete [] buffer;
        return true;
    } catch(...) {
        delete [] buffer;
        return false;
    }
}

void MPLSParser::parse(quint8* buffer, int len)
{
    BitStreamReader reader;
    try {
        reader.setBuffer(buffer, buffer + len);
        char m_type_indicator[5];
        char version_number[5];
        CLPIStreamInfo::readString(m_type_indicator, reader, 4);
        CLPIStreamInfo::readString(version_number, reader, 4);
        int playList_start_address = reader.getBits(32);
        int playListMark_start_address = reader.getBits(32);
        int extensionData_start_address =  reader.getBits(32);
        Q_UNUSED(extensionData_start_address);

        for (int i = 0; i < 5; i++)
            reader.skipBits(32); //reserved_for_future_use 160 bslbf
        AppInfoPlayList(reader);
        parsePlayList(buffer + playList_start_address, len - playList_start_address);
        /*
        for (int i=0; i<N1; i++) {
            padding_word 16 bslbf
        }
        PlayList();
        for (int i=0; i<N2; i++) {
            padding_word 16 bslbf
        }
        */
        parsePlayListMark(buffer + playListMark_start_address, len - playListMark_start_address);
        /*
        for (i=0; i<N3; i++) {
            padding_word 16 bslbf
        }
        ExtensionData();
        for (i=0; i<N4; i++) {
            padding_word 16 bslbf
        }
        */
    } catch(BitStreamException& e) {
        THROW_BITSTREAM_ERR2("Can't parse media playlist file: unexpected end of data");
    }
}

int MPLSParser::compose(quint8* buffer, int bufferSize, MPLSParser::DiskType dt)
{
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (m_streamInfo[i].m_isSecondary && (stream_coding_type==0x02 || stream_coding_type==0x1B || stream_coding_type==0xEA))
            m_number_of_SubPaths = 1;
    }


    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + bufferSize);

    QString m_type_indicator = QLatin1String("MPLS");
    QString version_number;
    if (dt == DT_BLURAY)
        version_number = QLatin1String("0200");
    else
        version_number = QLatin1String("0100");
    CLPIStreamInfo::writeString(m_type_indicator.toLatin1(), writer, 4);
    CLPIStreamInfo::writeString(version_number.toLatin1(), writer, 4);
    quint32* playList_bit_pos = (quint32*) (buffer + writer.getBitsCount()/8);
    writer.putBits(32,0);
    quint32* playListMark_bit_pos = (quint32*) (buffer + writer.getBitsCount()/8);
    writer.putBits(32,0);
    quint32* extDataStartAddr = (quint32*) (buffer + writer.getBitsCount()/8);
    writer.putBits(32,0); // extension data start address
    for (int i = 0; i < 5; i++)
        writer.putBits(32,0); //reserved_for_future_use 160 bslbf
    composeAppInfoPlayList(writer);
    while (writer.getBitsCount() % 16 != 0)
        writer.putBits(8,0);
    *playList_bit_pos = htonl(writer.getBitsCount()/8);
    composePlayList(writer);


    while (writer.getBitsCount() % 16 != 0)
        writer.putBits(8,0);
    *playListMark_bit_pos = htonl(writer.getBitsCount()/8);
    composePlayListMark(writer);

    while (writer.getBitsCount() % 16 != 0)
        writer.putBits(8,0);

    if (m_number_of_SubPaths > 0)
    {
        *extDataStartAddr = htonl(writer.getBitsCount()/8);
        quint8 buffer[1024*4];
        MPLSStreamInfo& mainStreamInfo = getMainStream();
        QVector<ExtDataBlockInfo> blockVector;

        for (int i = 0; i < m_number_of_SubPaths; ++i)
        {
            int bufferSize = composePip_metadata(buffer, sizeof(buffer), i, mainStreamInfo.m_index);
            ExtDataBlockInfo extDataBlock(buffer, bufferSize, 1, 1);
            blockVector.push_back(extDataBlock);
        }
        composeExtensionData(writer, blockVector);
        while (writer.getBitsCount() % 16 != 0)
            writer.putBits(8,0);
    }

    writer.flushBits();
    return writer.getBitsCount()/8;

}

void MPLSParser::AppInfoPlayList(BitStreamReader& reader)
{
    quint32 length = reader.getBits(32);
    Q_UNUSED(length);
    reader.skipBits(8); //reserved_for_future_use 8 bslbf
    m_PlayList_playback_type = reader.getBits(8); //8 bslbf
    if (m_PlayList_playback_type==2 || m_PlayList_playback_type==3) { // 1 == Sequential playback of PlayItems
        m_playback_count = reader.getBits(16); //16 uimsbf
    } else {
        reader.skipBits(16); //reserved_for_future_use 16 bslbf
    }
    UO_mask_table(reader);
    /*
    bool PlayList_random_access_flag = reader.getBit(); //1 bslbf
    bool audio_mix_app_flag = reader.getBits(1);
    bool lossless_may_bypass_mixer_flag = reader.getBit(); //1 bslbf
    reader.skipBits(13); //reserved_for_future_use 13 bslbf
    */
    reader.skipBits(16); //sum of all above
}

void MPLSParser::composeAppInfoPlayList(BitStreamWriter& writer)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8,0); //reserved_for_future_use 8 bslbf
    writer.putBits(8, m_PlayList_playback_type); //8 bslbf
    if (m_PlayList_playback_type==2 || m_PlayList_playback_type==3)  // 1 == Sequential playback of PlayItems
        writer.putBits(16, m_playback_count); //16 uimsbf
    else
        writer.putBits(16, 0); //reserved_for_future_use 16 bslbf

    writer.putBits(32,0); //UO_mask_table;
    writer.putBits(32,0); //UO_mask_table cont;
    writer.putBit(0); // PlayList_random_access_flag
    writer.putBit(0); // audio_mix_app_flag. 0 == no secondary audio
    writer.putBit(0); // lossless_may_bypass_mixer_flag
    writer.putBits(13,0); //reserved_for_future_use 13 bslbf
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

void MPLSParser::UO_mask_table(BitStreamReader& reader)
{
    reader.skipBit(); //reserved_for_future_use // reserved for menu call mask 1 bslbf
    reader.skipBit(); //reserved_for_future_use // reserved for title search mask 1 bslbf
    m_chapter_search_mask = reader.getBit();//1 bslbf
    m_time_search_mask = reader.getBit(); //1 bslbf
    m_skip_to_next_point_mask = reader.getBit(); //1 bslbf
    m_skip_back_to_previous_point_mask = reader.getBit(); //1 bslbf
    reader.skipBit(); //reserved_for_future_use // reserved for play FirstPlay mask 1 bslbf
    m_stop_mask = reader.getBit(); //1 bslbf
    m_pause_on_mask = reader.getBit(); //1 bslbf
    reader.skipBit(); //reserved_for_future_use // reserved for pause off mask 1 bslbf
    m_still_off_mask = reader.getBit(); //1 bslbf
    m_forward_play_mask = reader.getBit(); //1 bslbf
    m_backward_play_mask = reader.getBit(); //1 bslbf
    m_resume_mask = reader.getBit(); //1 bslbf
    m_move_up_selected_button_mask = reader.getBit(); //1 bslbf
    m_move_down_selected_button_mask = reader.getBit(); //1 bslbf
    m_move_left_selected_button_mask = reader.getBit(); //1 bslbf
    m_move_right_selected_button_mask = reader.getBit(); //1 bslbf
    m_select_button_mask = reader.getBit(); //1 bslbf
    m_activate_button_mask = reader.getBit(); //1 bslbf
    m_select_button_and_activate_mask = reader.getBit(); //1 bslbf
    m_primary_audio_stream_number_change_mask = reader.getBit(); //1 bslbf
    reader.skipBit(); //reserved_for_future_use 1 bslbf
    m_angle_number_change_mask = reader.getBit(); //1 bslbf
    m_popup_on_mask = reader.getBit(); //1 bslbf
    m_popup_off_mask = reader.getBit(); //1 bslbf
    m_PG_textST_enable_disable_mask = reader.getBit(); //1 bslbf
    m_PG_textST_stream_number_change_mask = reader.getBit(); //1 bslbf
    m_secondary_video_enable_disable_mask = reader.getBit(); //1 bslbf
    m_secondary_video_stream_number_change_mask = reader.getBit(); //1 bslbf
    m_secondary_audio_enable_disable_mask = reader.getBit(); //1 bslbf
    m_secondary_audio_stream_number_change_mask = reader.getBit(); //
    reader.skipBit(); //reserved_for_future_use 1 bslbf
    m_PiP_PG_textST_stream_number_change_mask = reader.getBit(); //1 bslbf
    reader.skipBits(30); //reserved_for_future_use 30 bslbf
}

void MPLSParser::parsePlayList(quint8* buffer, int len)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, buffer + len);
    /*quint32 length =*/ reader.skipBits(32);
    reader.skipBits(16); //reserved_for_future_use 16 bslbf
    int number_of_PlayItems = reader.getBits(16);//16 uimsbf
    m_number_of_SubPaths = reader.getBits(16); //16 uimsbf
    for (int PlayItem_id=0; PlayItem_id<number_of_PlayItems; PlayItem_id++)
        parsePlayItem(reader);
    for (int SubPath_id=0; SubPath_id<m_number_of_SubPaths; SubPath_id++) {
        //SubPath(); // not implemented now
    }
}

MPLSStreamInfo& MPLSParser::getMainStream()
{
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int coding_type = m_streamInfo[i].m_stream_coding_type;
        if (coding_type==0x02 || coding_type==0x1B || coding_type==0xEA)
            return m_streamInfo[i];
    }
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int coding_type = m_streamInfo[i].m_stream_coding_type;
        if (coding_type==0x80 || coding_type==0x81 ||
            coding_type==0x82 || coding_type==0x83 ||
            coding_type==0x84 || coding_type==0x85 ||
            coding_type==0x86 || coding_type==0xA1 ||
            coding_type==0xA2)
        {
            return m_streamInfo[i];
        }
    }
    THROW_BITSTREAM_ERR2("Can't find stream index. One audio or video stream is needed.");
}

void MPLSParser::composePlayList(BitStreamWriter& writer) {
    //quint32 length = reader.getBits(32);
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount()/8;
    writer.putBits(16,0); //reserved_for_future_use 16 bslbf
    MPLSStreamInfo& mainStreamInfo = getMainStream();
    writer.putBits(16, mainStreamInfo.m_index.size());//16 uimsbf number_of_PlayItems
    writer.putBits(16, m_number_of_SubPaths ? mainStreamInfo.m_index.size() : 0); //m_number_of_SubPaths
    //connection_condition = 1;
    for (int PlayItem_id = 0; PlayItem_id < mainStreamInfo.m_index.size(); PlayItem_id++)
    {
        composePlayItem(writer, PlayItem_id, mainStreamInfo.m_index);
        //connection_condition = 6;
    }
    for (int SubPath_id=0; SubPath_id < m_number_of_SubPaths * mainStreamInfo.m_index.size(); SubPath_id++)
    {
        composeSubPath(writer, SubPath_id, mainStreamInfo.m_index);
    }
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

void MPLSParser::composeSubPath(BitStreamWriter& writer, int /*playItemNum*/, QVector<PMTIndex>& pmtIndexList)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount()/8;

    writer.putBits(8, 0); // reserved_for_future_use
    writer.putBits(8, 7); // SubPath_type = 7 (In-mux and Synchronous type of Picture-in-Picture)
    writer.putBits(15, 0); // reserved_for_future_use
    writer.putBits(1, 0); // is_repeat_SubPath = false
    writer.putBits(8, 0); // reserved_for_future_use

    /*
    QVector<StreamInfo*> secondary;
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (m_streamInfo[i].isSecondary && (stream_coding_type==0x02 || stream_coding_type==0x1B || stream_coding_type==0xEA))
            secondary.push_back(m_streamInfo[i]);
    }
    */
    writer.putBits(8, pmtIndexList.size()); // number_of_SubPlayItems
    for (int i = 0; i < pmtIndexList.size(); i++)
        composeSubPlayItem(writer,  i, pmtIndexList);
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

void MPLSParser::composeSubPlayItem(BitStreamWriter& writer, int playItemNum, QVector<PMTIndex>& pmtIndexList)
{
    quint16* lengthPos = (quint16*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(16,0); // skip lengthField
    int beforeCount = writer.getBitsCount()/8;

    QString clip_Information_file_name = strPadLeft(QString::number(playItemNum + m_m2tsOffset),5, '0');
    CLPIStreamInfo::writeString(clip_Information_file_name.toLatin1(), writer, 5);
    char clip_codec_identifier[] = "M2TS";
    CLPIStreamInfo::writeString(clip_codec_identifier, writer, 4);
    int connection_condition = playItemNum == 0 ? 1 : 6;
    writer.putBits(27, 0); // reserved_for_future_use
    writer.putBits(4, connection_condition); //4 bslbf
    writer.putBit(0); // is_multi_Clip_entries
    writer.putBits(8, m_ref_to_STC_id);//8 uimsbf

    if (playItemNum == 0)
        writer.putBits(32, m_IN_time);//32 uimsbf
    else if (pmtIndexList[playItemNum-1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum].begin().key()/2);
    else
        writer.putBits(32, m_IN_time);//32 uimsbf

    if (playItemNum == pmtIndexList.size()-1)
        writer.putBits(32, m_OUT_time); //32 uimsbf
    else if (pmtIndexList[playItemNum+1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum+1].begin().key()/2); //32 uimsbf
    else
        writer.putBits(32, m_OUT_time  ); //32 uimsbf

    writer.putBits(16, playItemNum); // sync_PlayItem_id. reference to play_item id. Now I use zerro value
    //sync_start_PTS_of_PlayItem
    if (playItemNum == 0)
        writer.putBits(32, m_IN_time);//32 uimsbf
    else if (pmtIndexList[playItemNum-1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum].begin().key()/2);
    else
        writer.putBits(32, m_IN_time);//32 uimsbf

    //writer.flushBits();
    *lengthPos = htons(writer.getBitsCount()/8 - beforeCount);
}

int MPLSParser::composePip_metadata(quint8* buffer, int bufferSize, int /*subPathId*/, QVector<PMTIndex>& pmtIndexList)
{
    // The ID1 value and the ID2 value of the ExtensionData() shall be set to 0x0001 and 0x0001
    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + bufferSize);
    quint32* lengthPos = (quint32*) buffer;
    writer.putBits(32, 0); //int length = reader.getBits(8); //8 uimsbf

    int pipCnt = 0;
    QVector<int> pipVSize;
    int mainVSize = 0;
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (stream_coding_type==0x02 || stream_coding_type==0x1B || stream_coding_type==0xEA)
        {
            if (m_streamInfo[i].m_isSecondary)
            {
                pipVSize << m_streamInfo[i].m_height;
                pipCnt++;
            }
            else {
                mainVSize = m_streamInfo[i].m_height;
            }
        }
    }

    writer.putBits(16, pipCnt * pmtIndexList.size());
    QVector<quint32*> blockDataAddressPos;
    for (int i = 0; i < pmtIndexList.size(); ++i)
    {
        for (int k = 0; k < pipCnt; k++)
        {
            //metadata_block_header[k]() {
            writer.putBits(16, i); // ref_to_PlayItem_id
            writer.putBits(8, k); // ref_to_secondary_video_stream_id
            writer.putBits(8, 0); // reserved_for_future_use
            writer.putBits(4, 1); // pip_timeline_type == 1. Synchronous type of Picture-in-Picture
            writer.putBit(0); // is_luma_key = 0
            writer.putBit(1); // trick_playing_flag. keep PIP windows when tricking
            writer.putBits(10,0); // reserved_for_word_align
            //if (is_luma_key==1b) {
            //    reserved_for_future_use 8 bslbf
            //    upper_limit_luma_key 8 uimsbf
            //} else {
                writer.putBits(16,0); // reserved_for_future_use 16 bslbf
            //}
            writer.putBits(16,0); // reserved_for_future_use 16 bslbf
            blockDataAddressPos.push_back((quint32*) (writer.getBuffer() + writer.getBitsCount()/8));
            writer.putBits(32,0); // metadata_block_data_start_address
            //}
        }
    }
    while (writer.getBitsCount() % 16 != 0)
        writer.putBit(0);
    for (int i = 0; i < pmtIndexList.size(); ++i)
    {
        for (int k = 0; k < pipCnt; k++)
        {
            *(blockDataAddressPos[i*pipCnt + k]) = htonl(writer.getBitsCount()/8);

            writer.putBits(16, 1); // number_of_pip_metadata_entries
            {
                if (i == 0)
                    writer.putBits(32, m_IN_time);//32 uimsbf
                else if (pmtIndexList[i-1].size() > 0)
                    writer.putBits(32, pmtIndexList[i].begin().key()/2);
                else
                    writer.putBits(32, m_IN_time);//32 uimsbf
                writer.putBits(12, 0); //pip_horizontal_position[i] 12 uimsbf
                writer.putBits(12, (mainVSize - pipVSize[k]) / 2);  //pip_vertical_position[i] 12 uimsbf. todo: remove constant here!!!
                writer.putBits(4, 1);  //pip_scale[i] 4 uimsbf. 1 == no_scale
                writer.putBits(4, 0); // reserved_for_future_use 4 bslbf
            }
            while (writer.getBitsCount() % 16 != 0)
                writer.putBit(0);
        }
    }

    writer.flushBits();
    *lengthPos = htonl(writer.getBitsCount()/8 - 4);
    return writer.getBitsCount()/8 ;
}

void MPLSParser::composeExtensionData(BitStreamWriter& writer, QVector<ExtDataBlockInfo>& extDataBlockInfo)
{
    QVector<quint32*> extDataStartAddrPos;
    extDataStartAddrPos.resize(extDataBlockInfo.size());
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32, 0); //int length = reader.getBits(8); //8 uimsbf
    int initPos = writer.getBitsCount()/8;
    if (extDataBlockInfo.size() > 0)
    {
        writer.putBits(32, 0); //data_block_start_address
        writer.putBits(24, 0); // reserved_for_word_align
        writer.putBits(8, extDataBlockInfo.size());
        for (int i = 0; i < extDataBlockInfo.size(); i++) {
            writer.putBits(16, extDataBlockInfo[i].m_id1);
            writer.putBits(16, extDataBlockInfo[i].m_id2);
            extDataStartAddrPos[i] = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
            writer.putBits(32, 0); // ext_data_start_address
            writer.putBits(32, extDataBlockInfo[i].m_dataLen); // ext_data_length
        }
        while ((writer.getBitsCount()/8- initPos) % 4 != 0)
            writer.putBits(16, 0);
        *(lengthPos+1) = htonl(writer.getBitsCount()/8 - initPos + 4); // data_block_start_address
        for (int i = 0; i < extDataBlockInfo.size(); i++)
        {
            *(extDataStartAddrPos[i]) = htonl(writer.getBitsCount()/8 - initPos + 4);
            for (int j = 0; j < extDataBlockInfo[i].m_dataLen; ++j)
                writer.putBits(8, extDataBlockInfo[i].m_data[j]);
        }
    }

    *lengthPos = htonl(writer.getBitsCount()/8 - initPos);
}

void MPLSParser::parsePlayItem(BitStreamReader& reader)
{
    MPLSPlayItem newItem;
    /*int length =*/ reader.skipBits(16);
    char clip_Information_file_name[6];
    char clip_codec_identifier[5];
    CLPIStreamInfo::readString(clip_Information_file_name, reader, 5);
    newItem.m_fileName = QFile::decodeName(clip_Information_file_name);
    CLPIStreamInfo::readString(clip_codec_identifier, reader, 4);
    m_fileExt = QString::fromLatin1(clip_codec_identifier).trimmed();
    reader.skipBits(11); // reserved_for_future_use 11 bslbf
    m_is_multi_angle = reader.getBit();
    newItem.m_connection_condition = reader.getBits(4); //4 bslbf
    m_ref_to_STC_id = reader.getBits(8);//8 uimsbf

    newItem.m_IN_time = reader.getBits(32);//32 uimsbf
    newItem.m_OUT_time = reader.getBits(32); //32 uimsbf
    m_playItems.push_back(newItem);

    UO_mask_table(reader);
    m_PlayItem_random_access_flag = reader.getBit(); //1 bslbf
    reader.skipBits(7); //reserved_for_future_use 7 bslbf
    quint8 still_mode = reader.getBits(8); //8 bslbf
    if (still_mode == 0x01)
        /*int still_time =*/ reader.skipBits(16); //16 uimsbf
    else
        reader.skipBits(16); //reserved_for_future_use 16 bslbf

    if (m_is_multi_angle == 1)
    {
        m_number_of_angles = reader.getBits(8); // uimsbf
        reader.skipBits(6); //reserved_for_future_use 6 bslbf
        m_is_different_audios = reader.getBit();//1 bslbf
        m_is_seamless_angle_change = reader.getBit();//1 bslbf
        for (int angle_id = 1; // angles except angle_id=0
             angle_id<m_number_of_angles; angle_id++)
        {
            CLPIStreamInfo::readString(clip_Information_file_name, reader, 5); //8*5 bslbf
            CLPIStreamInfo::readString(clip_codec_identifier, reader,4); // 8*4 bslbf
            m_ref_to_STC_id = reader.getBits(8); // 8 uimsbf
        }
    }
    STN_table(reader);
}

void MPLSParser::composePlayItem(BitStreamWriter& writer, int playItemNum, QVector<PMTIndex>& pmtIndexList)
{
    quint16* lengthPos = (quint16*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(16,0); // skip lengthField
    int beforeCount = writer.getBitsCount()/8;
    QString clip_Information_file_name = strPadLeft(QString::number(playItemNum + m_m2tsOffset),5, '0');
    CLPIStreamInfo::writeString(clip_Information_file_name.toLatin1(), writer, 5);
    char clip_codec_identifier[] = "M2TS";
    CLPIStreamInfo::writeString(clip_codec_identifier, writer, 4);
    writer.putBits(11, 0); // reserved_for_future_use 11 bslbf
    writer.putBit(0); // m_is_multi_angle
    int connection_condition = playItemNum == 0 ? 1 : 6;
    writer.putBits(4, connection_condition); //4 bslbf
    writer.putBits(8, m_ref_to_STC_id);//8 uimsbf

    if (playItemNum == 0)
        writer.putBits(32, m_IN_time);//32 uimsbf
    else if (pmtIndexList[playItemNum-1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum].begin().key()/2);
    else
        writer.putBits(32, m_IN_time);//32 uimsbf

    if (playItemNum == pmtIndexList.size()-1)
        writer.putBits(32, m_OUT_time); //32 uimsbf
    else if (pmtIndexList[playItemNum+1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum+1].begin().key()/2); //32 uimsbf
    else
        writer.putBits(32, m_OUT_time); //32 uimsbf

    writer.putBits(32,0); //UO_mask_table;
    writer.putBits(32,0); //UO_mask_table cont;

    writer.putBit(m_PlayItem_random_access_flag); //1 bslbf
    writer.putBits(7,0); //reserved_for_future_use 7 bslbf
    writer.putBits(8, 0); // still_mode
    writer.putBits(16,0); // reserved after stillMode != 0x01
    composeSTN_table(writer);
    *lengthPos = htons(writer.getBitsCount()/8 - beforeCount);
}

void MPLSParser::parsePlayListMark(quint8* buffer, int len)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, buffer + len);
    quint32 length = reader.getBits(32); //
    Q_UNUSED(length);
    int number_of_PlayList_marks = reader.getBits(16); //16 uimsbf
    for(int PL_mark_id=0; PL_mark_id<number_of_PlayList_marks; PL_mark_id++)
    {
        reader.skipBits(8); //reserved_for_future_use 8 bslbf
        int mark_type = reader.getBits(8); //8 bslbf
        int ref_to_PlayItem_id = reader.getBits(16);
        quint32 mark_time_stamp = reader.getBits(32); //32 uimsbf
        int entry_ES_PID = reader.getBits(16); //16 uimsbf
        quint32 duration = reader.getBits(32); // 32 uimsbf
        Q_UNUSED(entry_ES_PID);
        Q_UNUSED(duration);
        if (mark_type == 1) //mark_type 0x01 = Chapter search
            m_marks.push_back(PlayListMark(ref_to_PlayItem_id, mark_time_stamp));
    }
}

int MPLSParser::calcPlayItemID(MPLSStreamInfo& streamInfo, quint32 pts)
{
    for (int i = 0; i < streamInfo.m_index.size(); i++)
    {
        if (streamInfo.m_index[i].size() > 0)
        {
            if (streamInfo.m_index[i].begin().key() > pts)
                return FFMAX(i-1,0);
        }
    }
    return streamInfo.m_index.size()-1;
}

void MPLSParser::composePlayListMark(BitStreamWriter& writer)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;
    MPLSStreamInfo& streamInfo = MPLSParser::getMainStream();
    if (m_marks.size() == 0)
    {
        if (m_chapterLen == 0)
        {
            m_marks.push_back(PlayListMark(-1, m_IN_time));
        }
        else {
            for (unsigned i = m_IN_time; i < m_OUT_time; i += m_chapterLen*45000)
                m_marks.push_back(PlayListMark(-1,i));
        }
    }
    writer.putBits(16, m_marks.size()); //16 uimsbf
    for(int i = 0; i < m_marks.size(); i++)
    {
        writer.putBits(8,0); //reserved_for_future_use 8 bslbf
        writer.putBits(8, 1); //mark_type 0x01 = Chapter search
        if (m_marks[i].m_playItemID >= 0)
            writer.putBits(16, m_marks[i].m_playItemID); // play item ID
        else
            writer.putBits(16, calcPlayItemID(streamInfo, m_marks[i].m_markTime*2)); // play item ID
        writer.putBits(32, m_marks[i].m_markTime); //32 uimsbf
        writer.putBits(16,0xffff); //entry_ES_PID always 0xffff for mark_type 1
        writer.putBits(32, 0); // duration always 0 for mark_type 1
    }
    *lengthPos = htonl(writer.getBitsCount()/8 - beforeCount);
}

/*
void MPLSParser::composePlayListMark(BitStreamWriter& writer)
{
    quint32* lengthPos = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip lengthField
    int beforeCount = writer.getBitsCount()/8;

    writer.putBits(16, 1); // number_of_PlayList_marks
    //for(int PL_mark_id=0; PL_mark_id<number_of_PlayList_marks; PL_mark_id++)
    //{
        writer.putBits(8, 0 ); //reserved_for_future_use 8 bslbf
        writer.putBits(8, 1); // mark_type

        writer.putBits(16, 0); // ref_to_PlayItem_id
        writer.putBits(32, m_IN_time); // mark_time_stamp
        writer.putBits(16, 0xffff); // entry_ES_PID
        writer.putBits(32, 0); // duration

    //}
    *lengthPos = ntohl(writer.getBitsCount()/8 - beforeCount);
}
*/

void MPLSParser::composeSTN_table(BitStreamWriter& writer)
{
    quint16* lengthPos = (quint16*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(16,0); // skip lengthField
    int beforeCount = writer.getBitsCount()/8;
    writer.putBits(16,0); //reserved_for_future_use 16 bslbf

    int number_of_primary_video_stream_entries = 0;
    int number_of_secondary_video_stream_entries = 0;
    int number_of_primary_audio_stream_entries = 0;
    int number_of_PG_textST_stream_entries = 0;
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (stream_coding_type==0x02 || stream_coding_type==0x1B || stream_coding_type==0xEA)
        {
            if (!m_streamInfo[i].m_isSecondary)
                number_of_primary_video_stream_entries++;
            else
                number_of_secondary_video_stream_entries++;
        }
        else if (stream_coding_type==0x80 || stream_coding_type==0x81   || stream_coding_type==0x82 ||
                   stream_coding_type==0x83 || stream_coding_type==0x84 || stream_coding_type==0x85 ||
                   stream_coding_type==0x86 || stream_coding_type==0xA1 || stream_coding_type==0xA2)
        {
            number_of_primary_audio_stream_entries++;
        }
        else if (stream_coding_type==0x90) {
            number_of_PG_textST_stream_entries++;
        }
        else
        {
            QString errStr;
            errStr += QLatin1String("Unsupported media type ");
            errStr += QString::number(stream_coding_type);
            errStr += QLatin1String(" for AVCHD/Blu-ray muxing");
            THROW_BITSTREAM_ERR2(errStr);
        }
    }

    writer.putBits(8,number_of_primary_video_stream_entries); //8 uimsbf // test subpath

    writer.putBits(8,number_of_primary_audio_stream_entries);
    writer.putBits(8,number_of_PG_textST_stream_entries);
    writer.putBits(8,0); //int number_of_IG_stream_entries = reader.getBits(8); //8 uimsbf
    writer.putBits(8,0);//int number_of_secondary_audio_stream_entries = reader.getBits(8); //8 uimsbf
    writer.putBits(8, number_of_secondary_video_stream_entries);  //int number_of_secondary_video_stream_entries. Now subpath used for second video stream
    writer.putBits(8,0);//int number_of_PiP_PG_textST_stream_entries_plus = reader.getBits(8); //8 uimsbf
    writer.putBits(32,0); //reserved_for_future_use 40 bslbf
    writer.putBits(8,0);

    //if (m_number_of_SubPaths == 0)
    for (int i = 0; i < number_of_primary_video_stream_entries; i++)
    {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (!m_streamInfo[i].m_isSecondary && (stream_coding_type==0x02 || stream_coding_type==0x1B || stream_coding_type==0xEA))
        {
            m_streamInfo[i].composeStreamEntry(writer);
            m_streamInfo[i].composeStreamAttributes(writer);
        }
    }

    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (stream_coding_type==0x80 || stream_coding_type==0x81 || stream_coding_type==0x82 ||
            stream_coding_type==0x83 || stream_coding_type==0x84 || stream_coding_type==0x85 ||
            stream_coding_type==0x86 || stream_coding_type==0xA1 || stream_coding_type==0xA2)
        {
            m_streamInfo[i].composeStreamEntry(writer);
            m_streamInfo[i].composeStreamAttributes(writer);
        }
    }

    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (stream_coding_type==0x90)
        {
            m_streamInfo[i].composeStreamEntry(writer);
            m_streamInfo[i].composeStreamAttributes(writer);
        }
    }

    for (int i = 0; i < m_streamInfo.size(); i++) {
        int stream_coding_type = m_streamInfo[i].m_stream_coding_type;
        if (m_streamInfo[i].m_isSecondary && (stream_coding_type==0x02 || stream_coding_type==0x1B || stream_coding_type==0xEA))
        {
            m_streamInfo[i].m_type = 3;
            m_streamInfo[i].composeStreamEntry(writer);
            m_streamInfo[i].composeStreamAttributes(writer);
            //comb_info_Secondary_video_Secondary_audio() {
                writer.putBits(8, 0); // number_of_secondary_audio_ref_entries
                writer.putBits(8, 0);  // reserved_for_word_align
            //}
            //comb_info_Secondary_video_PiP_PG_textST(){
                writer.putBits(8, 0); // number_of_PiP_PG_textST_ref_entries
                writer.putBits(8, 0);  // reserved_for_word_align
            //}
        }
    }

    *lengthPos = htons(writer.getBitsCount()/8 - beforeCount);
}

void MPLSParser::STN_table(BitStreamReader& reader)
{
    int length = reader.getBits(16); //16 uimsbf
    Q_UNUSED(length);
    reader.skipBits(16); //reserved_for_future_use 16 bslbf
    int number_of_primary_video_stream_entries = reader.getBits(8); //8 uimsbf
    int number_of_primary_audio_stream_entries = reader.getBits(8); //8 uimsbf
    int number_of_PG_textST_stream_entries = reader.getBits(8); //8 uimsbf
    int number_of_IG_stream_entries = reader.getBits(8); //8 uimsbf
    int number_of_secondary_audio_stream_entries = reader.getBits(8); //8 uimsbf
    int number_of_secondary_video_stream_entries = reader.getBits(8); //8 uimsbf
    int number_of_PiP_PG_textST_stream_entries_plus = reader.getBits(8); //8 uimsbf
    reader.skipBits(32); //reserved_for_future_use 40 bslbf
    reader.skipBits(8);

    for (int primary_video_stream_id=0; primary_video_stream_id< number_of_primary_video_stream_entries; primary_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        m_streamInfo.push_back(streamInfo);
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
    }
    for (int primary_audio_stream_id=0; primary_audio_stream_id < number_of_primary_audio_stream_entries; primary_audio_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        m_streamInfo.push_back(streamInfo);
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
    }

    for (int PG_textST_stream_id=0; PG_textST_stream_id<number_of_PG_textST_stream_entries+number_of_PiP_PG_textST_stream_entries_plus;PG_textST_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        m_streamInfo.push_back(streamInfo);
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
    }
    for (int IG_stream_id=0; IG_stream_id<number_of_IG_stream_entries; IG_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        m_streamInfo.push_back(streamInfo);
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
    }

    for (int secondary_audio_stream_id=0; secondary_audio_stream_id < number_of_secondary_audio_stream_entries; secondary_audio_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        m_streamInfo.push_back(streamInfo);
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        //comb_info_Secondary_audio_Primary_audio(){
        int number_of_primary_audio_ref_entries = reader.getBits(8); //8 uimsbf
        Q_UNUSED(number_of_primary_audio_ref_entries);
        reader.skipBits(8); //reserved_for_word_align  8 bslbf
        for (int i=0; i<number_of_primary_audio_ref_entries; i++)
            /*int primary_audio_stream_id_ref =*/ reader.skipBits(8); //8 uimsbf
        if (number_of_primary_audio_ref_entries%2==1)
            reader.skipBits(8); //reserved_for_word_align
    }
    for (int secondary_video_stream_id=0; secondary_video_stream_id < number_of_secondary_video_stream_entries; secondary_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        m_streamInfo.push_back(streamInfo);
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        //comb_info_Secondary_video_Secondary_audio(){
        int number_of_secondary_audio_ref_entries = reader.getBits(8); //8 uimsbf
        reader.skipBits(8); //reserved_for_word_align 8 bslbf

        for (int i=0; i<number_of_secondary_audio_ref_entries; i++) {
            int secondary_audio_stream_id_ref = reader.getBits(8); //8 uimsbf
            Q_UNUSED(secondary_audio_stream_id_ref);
        }
        if (number_of_secondary_audio_ref_entries%2==1)
            reader.skipBits(8); //reserved_for_word_align 8 bslbf
        //comb_info_Secondary_video_PiP_PG_textST(){
        int number_of_PiP_PG_textST_ref_entries = reader.getBits(8); //8 uimsbf
        reader.skipBits(8); //reserved_for_word_align 8 bslbf
        for (int i=0; i < number_of_PiP_PG_textST_ref_entries; i++) {
            int PiP_PG_textST_stream_id_ref = reader.getBits(8); //8 uimsbf
            Q_UNUSED(PiP_PG_textST_stream_id_ref);
        }
        if (number_of_PiP_PG_textST_ref_entries%2==1)
            reader.skipBits(8); //reserved_for_word_align 8 bslbf
    }
}

// ------------- M2TSStreamInfo -----------------------

M2TSStreamInfo::M2TSStreamInfo(const PMTStreamInfo& pmtStreamInfo)
{
    m_ref_to_stream_PID_of_mainClip = pmtStreamInfo.m_pid;
    m_stream_coding_type = pmtStreamInfo.m_streamType;
    m_index = pmtStreamInfo.m_index;
    m_character_code = 0;
    m_video_format = 0;
    m_frame_rate_index = 0;
    m_audio_presentation_type = 0;
    m_sampling_frequency_index = 0;
    m_aspect_ratio_index = 3; // 16:9; 2 = 4:3
    memset(m_language_code, 0, 4);
    m_character_code = 0;
    m_height = 0;
    m_isSecondary = pmtStreamInfo.m_isSecondary;
    memcpy(m_language_code, pmtStreamInfo.m_lang,3);
    //memcpy(m_language_code, "eng", 3); // todo: delete this line
    if (pmtStreamInfo.m_codecReader != 0)
    {
        BlurayVideoStream* vStream = dynamic_cast <BlurayVideoStream*> (pmtStreamInfo.m_codecReader);
        if (vStream)
        {
            m_height = vStream->streamHeight();
            int width = vStream->streamWidth();
            bool interlaced = vStream->isInterlaced();

            double fps = vStream->fps();
            bool isNtsc = width <= 854 && m_height <= 480 && (fabs(25-fps) >= 0.5 && fabs(50-fps) >= 0.5);
            bool isPal = width <= 1024 && m_height <= 576 && (fabs(25-fps) < 0.5 || fabs(50-fps) < 0.5);

            if (isNtsc)
                m_video_format = interlaced ? 1 : 3;
            else if (isPal)
                m_video_format = interlaced ? 2 : 7;
            else if (width >= 1300)
                m_video_format = interlaced ? 4 : 6; // as 1920x1080
            else
                m_video_format = 5;  // as 1280x720

            if (fabs(fps-23.976) < 1e-4)
                m_frame_rate_index = 1;
            else if (fabs(fps-24.0) < 1e-4)
                m_frame_rate_index = 2;
            else if (fabs(fps-25.0) < 1e-4)
                m_frame_rate_index = 3;
            else if (fabs(fps-29.97) < 1e-4)
                m_frame_rate_index = 4;
            else if (fabs(fps-50.0) < 1e-4)
                m_frame_rate_index = 6;
            else if (fabs(fps-59.94) < 1e-4)
                m_frame_rate_index = 7;

            if (vStream->streamAR() == BlurayVideoStream::AR_3_4 || vStream->streamAR() == BlurayVideoStream::AR_VGA)
                m_aspect_ratio_index = 2; // 4x3
        }
        BlurayAudioStream* aStream = dynamic_cast <BlurayAudioStream*> (pmtStreamInfo.m_codecReader);
        if (aStream)
        {
            m_audio_presentation_type = aStream->channels();
            if (m_audio_presentation_type == 2)
                m_audio_presentation_type = 3;
            else if (m_audio_presentation_type > 3)
                m_audio_presentation_type = 6;
            int freq = aStream->freq();
            // todo: add index 12 and 14. 12: 48Khz core, 192Khz mpl, 14: 48Khz core, 96Khz mlp
            switch(freq)
            {
                case 48000:
                    if (aStream->altFreq() == 96000)
                        m_sampling_frequency_index = 14;
                    else if (aStream->altFreq() == 192000)
                        m_sampling_frequency_index = 12;
                    else
                        m_sampling_frequency_index = 1;
                    break;
                case 96000:
                    if (aStream->altFreq() == 192000)
                        m_sampling_frequency_index = 12;
                    else
                        m_sampling_frequency_index = 4;
                    break;
                case 192000:
                    m_sampling_frequency_index = 5;
                    break;
            }
        }
    }
}


// -------------- MPLSStreamInfo -----------------------

MPLSStreamInfo::MPLSStreamInfo(const PMTStreamInfo& pmtStreamInfo):
    M2TSStreamInfo(pmtStreamInfo)
{
    m_type = 1;
}

void MPLSStreamInfo::parseStreamEntry(BitStreamReader& reader)
{
    /*int length =*/ reader.skipBits(8); //8 uimsbf
    m_type = reader.getBits(8); //8 bslbf
    if (m_type==1)
    {
        m_ref_to_stream_PID_of_mainClip = reader.getBits(16); //16 uimsbf
        reader.skipBits(32); //reserved_for_future_use 48 bslbf
        reader.skipBits(16);
    }
    else if (m_type==2)
    {
        /*int ref_to_SubPath_id = */reader.skipBits(8); //8 uimsbf
        /*int ref_to_subClip_entry_id =*/ reader.skipBits(8); //8 uimsbf
        /*int ref_to_stream_PID_of_subClip =*/ reader.skipBits(16); //16 uimsbf
        reader.skipBits(32); //reserved_for_future_use 32 bslbf
    }
    else if (m_type==3)
    {
        /*int ref_to_SubPath_id = */        reader.skipBits(8); //8 uimsbf
        m_ref_to_stream_PID_of_mainClip =   reader.getBits(16); //16 uimsbf
        reader.skipBits(32); //reserved_for_future_use 40 bslbf
        reader.skipBits(8);
    }
}

void MPLSStreamInfo::composeStreamEntry(BitStreamWriter& writer)
{
    quint8* lengthPos = writer.getBuffer() + writer.getBitsCount()/8;
    writer.putBits(8, 0); //int length = reader.getBits(8); //8 uimsbf
    int initPos = writer.getBitsCount()/8;
    writer.putBits(8, m_type); //8 bslbf
    if (m_type==1)
    {
        writer.putBits(16, m_ref_to_stream_PID_of_mainClip); //16 uimsbf
        writer.putBits(32, 0); //reserved_for_future_use 48 bslbf
        writer.putBits(16,0);
    }
    else if (m_type == 3)
    {
        writer.putBits(8, 0); //ref_to_SubPath_id. constant subPathID == 0
        writer.putBits(16, m_ref_to_stream_PID_of_mainClip); //16 uimsbf
        writer.putBits(32, 0); //reserved_for_future_use 48 bslbf
        writer.putBits(8,0);
    }
    else {
        THROW_BITSTREAM_ERR2 ("Unsupported media type for AVCHD/Blu-ray muxing");
    }
    *lengthPos = writer.getBitsCount()/8 - initPos;
}

void MPLSStreamInfo::parseStreamAttributes(BitStreamReader& reader)
{
    /*int length =*/  reader.skipBits(8); // 8 uimsbf
    m_stream_coding_type = reader.getBits(8); //8 bslbf
    if (m_stream_coding_type==0x02 || m_stream_coding_type==0x1B || m_stream_coding_type==0xEA)
    {
        m_video_format = reader.getBits(4); //4 bslbf
        m_frame_rate_index = reader.getBits(4); // 4 bslbf
        reader.skipBits(24); //reserved_for_future_use 24 bslbf
    }
    else if (m_stream_coding_type==0x80 || m_stream_coding_type==0x81 || m_stream_coding_type==0x82 ||
               m_stream_coding_type==0x83 || m_stream_coding_type==0x84 || m_stream_coding_type==0x85 ||
               m_stream_coding_type==0x86 || m_stream_coding_type==0xA1 || m_stream_coding_type==0xA2)
    {
        m_audio_presentation_type = reader.getBits(4); //4 bslbf
        m_sampling_frequency_index = reader.getBits(4); // 4 bslbf
        CLPIStreamInfo::readString(m_language_code,reader,3);
    }
    else if (m_stream_coding_type==0x90)
    {
        // Presentation Graphics stream
        CLPIStreamInfo::readString(m_language_code, reader, 3);
        reader.skipBits(8); //reserved_for_future_use 8 bslbf
    }
    else if (m_stream_coding_type==0x91)
    {
        // Interactive Graphics stream
        CLPIStreamInfo::readString(m_language_code, reader, 3);
        reader.skipBits(8); //reserved_for_future_use 8 bslbf
    }
    else if (m_stream_coding_type==0x92)
    {
        // Text subtitle stream
        m_character_code = reader.getBits(8); //8 bslbf
        CLPIStreamInfo::readString(m_language_code, reader, 3);
    }
}

void MPLSStreamInfo::composeStreamAttributes(BitStreamWriter& writer)
{
    quint8* lengthPos = writer.getBuffer() + writer.getBitsCount()/8;
    writer.putBits(8, 0); //int length = reader.getBits(8); //8 uimsbf
    int initPos = writer.getBitsCount()/8;

    writer.putBits(8, m_stream_coding_type); //8 bslbf
    if (m_stream_coding_type==0x02 || m_stream_coding_type==0x1B || m_stream_coding_type==0xEA)
    {
        writer.putBits(4, m_video_format); //4 bslbf
        writer.putBits(4, m_frame_rate_index); // 4 bslbf
        writer.putBits(24,0); //reserved_for_future_use 24 bslbf
    }
    else if (m_stream_coding_type==0x80 || m_stream_coding_type==0x81 || m_stream_coding_type==0x82 ||
               m_stream_coding_type==0x83 || m_stream_coding_type==0x84 || m_stream_coding_type==0x85 ||
               m_stream_coding_type==0x86 || m_stream_coding_type==0xA1 || m_stream_coding_type==0xA2)
    {
        writer.putBits(4, m_audio_presentation_type); //4 bslbf
        writer.putBits(4, m_sampling_frequency_index); // 4 bslbf
        CLPIStreamInfo::writeString(m_language_code,writer,3);
    }
    else if (m_stream_coding_type==0x90)
    {
        // Presentation Graphics stream
        CLPIStreamInfo::writeString(m_language_code, writer, 3);
        writer.putBits(8,0); //reserved_for_future_use 8 bslbf
    }
    else {
        THROW_BITSTREAM_ERR2 ("Unsupported media type for AVCHD/Blu-ray muxing");
    }
    *lengthPos = writer.getBitsCount()/8 - initPos;
}

bool MovieObject::parse(const char* fileName)
{
    QFile file(QFile::decodeName(fileName));
    if (!file.open(QFile::ReadOnly))
        return false;
    quint64 fileSize = file.size();
    quint8* buffer = new quint8[fileSize];
    if (!file.read( (char*) buffer, fileSize))
    {
        delete [] buffer;
        return false;
    }
    try {
        parse(buffer, fileSize);
        delete [] buffer;
        return true;
    } catch(...) {
        delete [] buffer;
        return false;
    }
}


void MovieObject::parse(quint8* buffer, int len)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, buffer + len);
    char m_type_indicator[5];
    char version_number[5];
    CLPIStreamInfo::readString(m_type_indicator, reader, 4);
    CLPIStreamInfo::readString(version_number, reader, 4);
    quint32 ExtensionData_start_address = reader.getBits(32); //32 uimsbf
    Q_UNUSED(ExtensionData_start_address);
    for (int i = 0; i < 7; i++)
        reader.skipBits(32); //reserved_for_future_use 224 bslbf
    parseMovieObjects(reader);
    //for (int i=0; i<N1; i++) padding_word 16 bslbf
    //parseExtensionData(buffer + ExtensionData_start_address, buffer+len);
    //for (i=0; i<N2; i++) padding_word 16 bslbf
}

int MovieObject::compose(quint8* buffer, int len, MPLSParser::DiskType dt)
{
    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + len);
    // char m_type_indicator[5];
    // char version_number[5];
    CLPIStreamInfo::writeString("MOBJ", writer, 4);
    if (dt == MPLSParser::DT_BLURAY)
        CLPIStreamInfo::writeString("0200", writer, 4);
    else
        CLPIStreamInfo::writeString("0100", writer, 4);
    writer.putBits(32,0); //quint32 ExtensionData_start_address
    for (int i = 0; i < 7; i++)
        writer.putBits(32,0); //reserved_for_future_use 224 bslbf
    //composeMovieObjects
    quint32* moLen = (quint32*) (writer.getBuffer() + writer.getBitsCount()/8);
    writer.putBits(32,0); // skip length field
    int beforeCount = writer.getBitsCount()/8;
    writer.putBits(32,0); // reserved
    writer.putBits(16,1); // number of movie objects
    // compose commands
    writer.putBit(1); // resume_intention_flag
    writer.putBit(1); // menu_call_mask
    writer.putBit(1); // title_search_mask
    writer.putBits(13,0); // reserved

    writer.putBits(16,1); // number_of_navigation_commands

    writer.putBits(32, 0x22800000); // navigation command: play playlist
    writer.putBits(32, 0); // play list number 0
    writer.putBits(32, 0); // second argument is not used

    *moLen = htonl(writer.getBitsCount()/8 - beforeCount);
    writer.flushBits();
    return writer.getBitsCount()/8;
}

void MovieObject::parseMovieObjects(BitStreamReader& reader)
{
    /*quint32 length =*/                    reader.getBits(32); //32 uimsbf
                                            reader.skipBits(32); //reserved_for_future_use 32 bslbf
    quint16 number_of_mobjs =               reader.getBits(16); //16 uimsbf
    for (int mobj_id=0; mobj_id < number_of_mobjs; mobj_id++)
    {
        /*
        bool resume_intention_flag = reader.getBit(); // 1 bslbf
        bool menu_call_mask = reader.getBit(); // 1 bslbf
        bool title_search_mask = reader.getBit(); // 1 bslbf 
        reader.skipBits(13); //reserved_for_word_align 13 bslbf
        */
        reader.skipBits(16); // sum of all above

        int number_of_navigation_commands = reader.getBits(16); //[mobj_id] 16 uimsbf
        for (int command_id=0; command_id < number_of_navigation_commands; command_id++)
            parseNavigationCommand(reader); //navigation_command[mobj_id][command_id] 96 bslbf
    }
}

void MovieObject::parseNavigationCommand(BitStreamReader& reader)
{
    // operation code 32 bits
    /*
    int Operand_Count = reader.getBits(3);
    int command_group = reader.getBits(2); // 0x2 = set
    int command_subGroup = reader.getBits(3); // 0x00 for set = set
    bool iFlag1 = reader.getBit();
    bool iFlag2 = reader.getBit(); // both false
    reader.skipBits(2); // reserved
    int Branch_Option = reader.getBits(4);
    reader.skipBits(4); // reserved
    int Compare_Option = reader.getBits(4);
    reader.skipBits(3); // reserved
    int set_options = reader.getBits(5);
    */

    quint32 command = reader.getBits(32);
    switch(command)
    {
        case 0x20010000:
            NX_LOG(QLatin1String("BDMO command: Goto (register arg)"), cl_logDEBUG1);
            break;
        case 0x20810000:
            NX_LOG(QLatin1String("BDMO command: Goto (const arg)"), cl_logDEBUG1);
            break;
    }

    // operand destination 32 bits
    // next syntax only if iFlag for operand == 0
    bool iFlag1 = false;
    bool iFlag2 = false;
    if (iFlag1 == 0)
    {
        bool reg_flag1 = reader.getBit(); // 0: General Purpose Register, 1: Player Status Registers
        Q_UNUSED(reg_flag1);
        reader.skipBits(19);
        int regigsterNumber1 = reader.getBits(12);
        regigsterNumber1 = regigsterNumber1;
    }
    else
    {
        int immediateValue = reader.getBits(32);
        immediateValue = immediateValue;
    }

    // operand source 32 bits
    // next syntax only if iFlag for operand == 0
    if (iFlag2 == 0)
    {
        bool reg_flag1 = reader.getBit(); // 0: General Purpose Register, 1: Player Status Registers
        Q_UNUSED(reg_flag1);
        reader.skipBits(19);
        int regigsterNumber1 = reader.getBits(12);
        regigsterNumber1 = regigsterNumber1;
    }
    else
    {
        int immediateValue = reader.getBits(32);
        immediateValue = immediateValue;
    }
}
