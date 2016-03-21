#ifndef __BLURAY_HELPER_H
#define __BLURAY_HELPER_H

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "../bitStream.h"


class AbstractBlurayStream
{
public:
    AbstractBlurayStream() {}
    virtual ~AbstractBlurayStream() {}
};

class BlurayVideoStream: public AbstractBlurayStream 
{
public:
    enum AspectRatio {AR_3_4, AR_16_9, AR_VGA, AR_UNKNOWN};

    BlurayVideoStream();
    int streamHeight()     const { return m_streamHeight; }
    int streamWidth()      const { return m_streamWidth;  }
    bool isInterlaced()    const { return m_isInterlaced; }
    float fps()            const { return m_fps; }
    AspectRatio streamAR() const { return m_streamAR; }
private:
    int m_streamHeight;
    int m_streamWidth;
    float m_fps;
    bool m_isInterlaced;
    AspectRatio m_streamAR;
};

class BlurayAudioStream: public AbstractBlurayStream
{
public:
    BlurayAudioStream();
    int freq()     const { return m_freq; }
    int altFreq()  const { return m_altFreq; }
    int channels() const { return m_channels; }
private:
    int m_freq;
    int m_altFreq;
    int m_channels;
};

struct BluRayCoarseInfo 
{
    BluRayCoarseInfo();
    BluRayCoarseInfo(quint32 coarsePts, quint32 fineRefID, quint32 pktCnt);

    quint32 m_coarsePts;
    quint32 m_fineRefID;
    quint32 m_pktCnt;
};

struct PMTIndexData 
{
    PMTIndexData(quint32 pktCnt, quint32 frameLen): m_pktCnt(pktCnt), m_frameLen(frameLen) {}
    quint32 m_pktCnt;
    quint32 m_frameLen;
};

typedef QMap<qint64, PMTIndexData> PMTIndex;

struct PMTStreamInfo
{
    PMTStreamInfo();
    PMTStreamInfo(int streamType, int pid, quint8* esInfoData, int esInfoLen, AbstractBlurayStream* codecReader, const QString& lang, bool secondary);

    int m_streamType;
    int m_pid;
    int m_esInfoLen;
    int m_pmtPID;
    quint8 m_esInfoData[128];
    char m_lang[4];
    bool m_priorityStream;
    bool m_isSecondary;
    QVector<PMTIndex> m_index; // blu-ray seek index. key=number of tsFrame. value=information about key frame
    AbstractBlurayStream* m_codecReader;
};

struct PS_system_header
{
    //bool deserialize(quint8* buffer, int buf_size);
};

struct PS_stream_map
{
    bool deserialize(quint8* buffer, int buf_size);
};

struct M2TSStreamInfo 
{
    M2TSStreamInfo() {}
    M2TSStreamInfo(const PMTStreamInfo& pmtStreamInfo);
    int m_ref_to_stream_PID_of_mainClip;
    int m_stream_coding_type; // ts type
    int m_video_format;
    int m_frame_rate_index;
    int m_height;
    int m_aspect_ratio_index;
    int m_audio_presentation_type;
    int m_sampling_frequency_index;
    int m_character_code;
    char m_language_code[4];
    bool m_isSecondary;
    QVector<PMTIndex> m_index;
};

struct CLPIStreamInfo: public M2TSStreamInfo 
{
    CLPIStreamInfo();
    CLPIStreamInfo(const PMTStreamInfo& pmtStreamInfo);
    void ISRC(BitStreamReader& reader);
    void parseStreamCodingInfo(BitStreamReader& reader);
    static void readString(char* dest, BitStreamReader& reader, int size);
    static void writeString(const char* dest, BitStreamWriter& writer, int size);
    void composeISRC(BitStreamWriter& writer) const;
    void composeStreamCodingInfo(BitStreamWriter& writer) const;
public:
    char m_country_code[3];
    char m_copyright_holder[4];
    char m_recording_year[3];
    char m_recording_number[6];
};

struct CLPIProgramInfo
{
    quint32 m_SPN_program_sequence_start;
    quint16 m_program_map_PID;
    quint8 m_number_of_streams_in_ps;
};

class CLPIParser 
{
public:
    void parse(quint8* buffer, int len);
    bool parse(const QString& fileName);
    int compose(quint8* buffer, int bufferSize);

    QVector<CLPIProgramInfo> m_programInfo;
    char m_type_indicator[5];
    char version_number[5];
    QMap<int, CLPIStreamInfo> m_streamInfo;
    quint8 m_clip_stream_type;
    quint8 m_application_type;
    bool m_is_ATC_delta;
    quint32 m_TS_recording_rate;
    quint32 m_number_of_source_packets;
    char m_format_identifier[5];
    quint32 m_presentation_start_time; 
    quint32 m_presentation_end_time; 
    int m_clpiNum;

private:
    void parseProgramInfo(quint8* buffer, quint8* end);
    void parseSequenceInfo(quint8* buffer, quint8* end);
    void parseCPI(quint8* buffer, quint8* end);
    void parseClipMark(quint8* buffer, quint8* end);
    void parseClipInfo(BitStreamReader& reader);
    void parseExtensionData(quint8* buffer, quint8* end);
    void TS_type_info_block(BitStreamReader& reader);
    void composeProgramInfo(BitStreamWriter& writer);
    void composeTS_type_info_block(BitStreamWriter& writer);
    void composeClipInfo(BitStreamWriter& writer);
    void composeSequenceInfo(BitStreamWriter& writer);
    void composeCPI(BitStreamWriter& writer);
    void composeClipMark(BitStreamWriter& writer);
    void composeEP_map(BitStreamWriter& writer);
    QVector<BluRayCoarseInfo> buildCoarseInfo(M2TSStreamInfo& streamInfo);
    void composeEP_map_for_one_stream_PID(BitStreamWriter& writer, M2TSStreamInfo& streamInfo);
};

struct MPLSStreamInfo: public M2TSStreamInfo 
{
    MPLSStreamInfo() {}
    MPLSStreamInfo(const PMTStreamInfo& pmtStreamInfo);

    void parseStreamAttributes(BitStreamReader& reader);
    void parseStreamEntry(BitStreamReader& reader);
    void composeStreamAttributes(BitStreamWriter& reader);
    void composeStreamEntry(BitStreamWriter& reader);

    int m_type; 
    quint8 m_character_code;
};

struct MPLSPlayItem 
{
    quint32 m_IN_time;
    quint32 m_OUT_time;
    QString m_fileName;
    int m_connection_condition;
};

struct PlayListMark 
{
    PlayListMark(): m_playItemID(0), m_markTime(0) {}
    PlayListMark(int playItemID, quint32 markTime): m_playItemID(playItemID), m_markTime(markTime) {}

    int m_playItemID;
    quint32 m_markTime;
};

struct ExtDataBlockInfo
{
    ExtDataBlockInfo():
        m_data(0), m_dataLen(0), m_id1(0), m_id2(0) {}
    ExtDataBlockInfo(quint8* a_data, int a_dataLen, int a_id1, int a_id2):
        m_data(a_data), m_dataLen(a_dataLen), m_id1(a_id1), m_id2(a_id2) {}

    quint8* m_data;
    int m_dataLen;
    int m_id1;
    int m_id2;
};

struct MPLSParser 
{
    enum DiskType {DT_NONE, DT_BLURAY, DT_AVCHD};
    static const int TIME_BASE = 45000;

    MPLSParser() :
        m_number_of_SubPaths(0),
        m_chapterLen(0),
        m_m2tsOffset(0)
    {}

    bool parse(const QString& fileName);
    void parse(quint8* buffer, int len);
    int compose(quint8* buffer, int bufferSize, DiskType dt);
    void deserializePlayList(const QString& str);

    //char m_type_indicator[5];
    //char version_number[5];
    int m_PlayList_playback_type;
    int m_playback_count;
    //int number_of_PlayItems;
    int m_number_of_SubPaths;
    //char clip_Information_file_name[6];
    //char clip_codec_identifier[5];
    bool m_is_multi_angle;
    //int connection_condition;
    int m_ref_to_STC_id;
    bool m_PlayItem_random_access_flag;
    int m_number_of_angles;
    bool m_is_different_audios;
    bool m_is_seamless_angle_change;
    int m_chapterLen;
    quint32 m_IN_time;
    quint32 m_OUT_time;
    QVector<MPLSPlayItem> m_playItems;

    bool m_chapter_search_mask;
    bool m_time_search_mask;
    bool m_skip_to_next_point_mask;
    bool m_skip_back_to_previous_point_mask;
    bool m_stop_mask;
    bool m_pause_on_mask;
    bool m_still_off_mask;
    bool m_forward_play_mask;
    bool m_backward_play_mask;
    bool m_resume_mask;
    bool m_move_up_selected_button_mask;
    bool m_move_down_selected_button_mask;
    bool m_move_left_selected_button_mask;
    bool m_move_right_selected_button_mask;
    bool m_select_button_mask;
    bool m_activate_button_mask;
    bool m_select_button_and_activate_mask;
    bool m_primary_audio_stream_number_change_mask;
    bool m_angle_number_change_mask;
    bool m_popup_on_mask;
    bool m_popup_off_mask;
    bool m_PG_textST_enable_disable_mask;
    bool m_PG_textST_stream_number_change_mask;
    bool m_secondary_video_enable_disable_mask;
    bool m_secondary_video_stream_number_change_mask;
    bool m_secondary_audio_enable_disable_mask;
    bool m_secondary_audio_stream_number_change_mask;
    bool m_PiP_PG_textST_stream_number_change_mask;
    QVector<MPLSStreamInfo> m_streamInfo;
    QVector <PlayListMark> m_marks;
    int m_m2tsOffset;
    QString m_fileExt;
private:
    void composeSubPlayItem(BitStreamWriter& writer, int playItemNum, QVector<PMTIndex>& pmtIndexList);
    void composeSubPath(BitStreamWriter& writer, int playItemNum, QVector<PMTIndex>& pmtIndexList);
    int composePip_metadata(quint8* buffer, int bufferSize, int secondVideoIndex, QVector<PMTIndex>& pmtIndexList);
    void composeExtensionData(BitStreamWriter& writer, QVector<ExtDataBlockInfo>& extDataBlockInfo);


    void AppInfoPlayList(BitStreamReader& reader);
    void composeAppInfoPlayList(BitStreamWriter& writer);
    void UO_mask_table(BitStreamReader& reader);
    void parsePlayList(quint8* buffer, int len);
    void parsePlayItem(BitStreamReader& reader);
    void parsePlayListMark(quint8* buffer, int len);
    void STN_table(BitStreamReader& reader);

    void composePlayList(BitStreamWriter& writer);
    //void composePlayItem(BitStreamWriter& writer);
    void composePlayItem(BitStreamWriter& writer, int playItemNum, QVector<PMTIndex>& pmtIndexList);
    void composePlayListMark(BitStreamWriter& writer);
    void composeSTN_table(BitStreamWriter& writer);
    MPLSStreamInfo& getMainStream();
    int calcPlayItemID(MPLSStreamInfo& streamInfo, quint32 pts);
};

class MovieObject 
{
public:
    bool parse(const char* fileName);
    void parse(quint8* buffer, int len);
    int compose(quint8* buffer, int len, MPLSParser::DiskType dt);
private:
    void parseMovieObjects(BitStreamReader& reader);
    void parseNavigationCommand(BitStreamReader& reader);
};

#endif //ENABLE_DATA_PROVIDERS

#endif
