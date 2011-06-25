#ifndef __BLURAY_HELPER_H
#define __BLURAY_HELPER_H

#include <QString>

#include "base/bitstream.h"

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

    BlurayVideoStream(): 
        m_streamHeight(0), m_streamWidth(0), m_fps(0.0), m_isInterlaced(false) {}
    int streamHeight() const { return m_streamHeight; }
    int streamWidth() const { return m_streamWidth; }
    bool isInterlaced() const { return m_isInterlaced; }
    float fps() const { return m_fps; }
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
    BlurayAudioStream(): 
      m_freq(0), m_altFreq(0), m_channels(0) {}
    int freq() const { return m_freq; }
    int altFreq() const { return m_altFreq; }
    int channels() const { return m_channels; }
private:
    int m_freq;
    int m_altFreq;
    int m_channels;
};

struct BluRayCoarseInfo 
{
    BluRayCoarseInfo(): 
        m_coarsePts(0), m_fineRefID(0), m_pktCnt(0) {}
    quint32 m_coarsePts;
    quint32 m_fineRefID;
    quint32 m_pktCnt;
    BluRayCoarseInfo(quint32 coarsePts, quint32 fineRefID, quint32 pktCnt):
        m_coarsePts(coarsePts), m_fineRefID(fineRefID), m_pktCnt(pktCnt) {}
};

struct PMTIndexData {
    quint32 m_pktCnt;
    quint32 m_frameLen;
    PMTIndexData(quint32 pktCnt, quint32 frameLen): m_pktCnt(pktCnt), m_frameLen(frameLen) {}
};

typedef QMap<qint64, PMTIndexData> PMTIndex;

struct 	PMTStreamInfo
{
    PMTStreamInfo() {m_streamType = 0; m_esInfoLen = 0; m_pid = 0; m_pmtPID = -1; priorityStream = false; isSecondary = false; /*m_calcStatistic = false;*/}
    PMTStreamInfo(int streamType, int pid, uint8_t* esInfoData, int esInfoLen, 
        AbstractBlurayStream* codecReader, const QString& lang, bool secondary) 
    {
        m_streamType = streamType;
        m_pid = pid;
        memcpy(m_esInfoData, esInfoData, esInfoLen);
        m_esInfoLen = esInfoLen;
        m_codecReader = codecReader;
        memset(m_lang, 0, sizeof(m_lang));
        memcpy(m_lang, lang.toAscii(), lang.size() < 3 ? lang.size() : 3);
        m_pmtPID = -1;
        priorityStream = false;
        isSecondary = secondary;
        //m_calcStatistic = false;
    }
    int m_streamType;
    int m_pid;
    int m_esInfoLen;
    int m_pmtPID;
    uint8_t m_esInfoData[128];
    char m_lang[4];
    bool priorityStream;
    bool isSecondary;
    // ---------------------
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

struct M2TSStreamInfo {
	M2TSStreamInfo() {}
	M2TSStreamInfo(const PMTStreamInfo& pmtStreamInfo);
	int ref_to_stream_PID_of_mainClip;
	int stream_coding_type; // ts type
	int video_format;
	int frame_rate_index;
	int height;
	int aspect_ratio_index;
	int audio_presentation_type;
	int sampling_frequency_index;
	int character_code;
	char language_code[4];
	bool isSecondary;
	QVector<PMTIndex> m_index;
};

struct CLPIStreamInfo: public M2TSStreamInfo 
{
	CLPIStreamInfo(const PMTStreamInfo& pmtStreamInfo): M2TSStreamInfo(pmtStreamInfo) {
		memset(country_code, 0, sizeof(country_code));
		memset(copyright_holder, 0, sizeof(copyright_holder));
		memset(recording_year, 0, sizeof(recording_year));
		memset(recording_number, 0, sizeof(recording_number));
	}
	//int stream_coding_type;
	//int video_format;
	//int frame_rate_index;
	//int aspect_ratio_index;
	//bool cc_flag;
	//int audio_presentation_type;
	//int sampling_frequency;

	//char language_code[4];
	char country_code[3];
	char copyright_holder[4];
	char recording_year[3];
	char recording_number[6];

	void ISRC(BitStreamReader& reader);
	void parseStreamCodingInfo(BitStreamReader& reader);
	static void readString(char* dest, BitStreamReader& reader, int size)  
	{
		for (int i = 0; i < size; i++)
			dest[i] = reader.getBits(8);
		dest[size] = 0;
	}
	static void writeString(const char* dest, BitStreamWriter& writer, int size)
	{
		for (int i = 0; i < size; i++)
			writer.putBits(8, dest[i]);
	}
	CLPIStreamInfo(): M2TSStreamInfo() {
		memset(language_code, 0, sizeof(language_code));
		memset(country_code, 0, sizeof(country_code));
		memset(copyright_holder, 0, sizeof(copyright_holder));
		memset(recording_year, 0, sizeof(recording_year));
		memset(recording_number, 0, sizeof(recording_number));
	}
	void composeISRC(BitStreamWriter& writer) const;
	void composeStreamCodingInfo(BitStreamWriter& writer) const;
private:
};

struct CLPIProgramInfo 
{
	quint32 SPN_program_sequence_start;
	quint16 program_map_PID;
	quint8 number_of_streams_in_ps;
};

class CLPIParser 
{
public:
	void parse(quint8* buffer, int len);
	bool parse(const QString& fileName);
	int compose(quint8* buffer, int bufferSize);
	QVector<CLPIProgramInfo> m_programInfo;

	char type_indicator[5];
	char version_number[5];
	QMap<int, CLPIStreamInfo> m_streamInfo;

	quint8 clip_stream_type;
	quint8 application_type;
	bool is_ATC_delta;
	quint32 TS_recording_rate;
	quint32 number_of_source_packets;
	char format_identifier[5];
	quint32 presentation_start_time; 
	quint32 presentation_end_time; 
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
	int type; 
	quint8 character_code;

	MPLSStreamInfo() {}
	MPLSStreamInfo(const PMTStreamInfo& pmtStreamInfo);

	void parseStreamAttributes(BitStreamReader& reader);
	void parseStreamEntry(BitStreamReader& reader);
	void composeStreamAttributes(BitStreamWriter& reader);
	void composeStreamEntry(BitStreamWriter& reader);
};

struct MPLSPlayItem 
{
	quint32 IN_time;
	quint32 OUT_time;
	QString fileName;
	int connection_condition;
};

struct PlayListMark 
{
	int m_playItemID;
	quint32 m_markTime;
    PlayListMark(): m_playItemID(0), m_markTime(0) {}
	PlayListMark(int playItemID, quint32 markTime): m_playItemID(playItemID), m_markTime(markTime) {}
};


struct ExtDataBlockInfo
{
    ExtDataBlockInfo():
        data(0), dataLen(0), id1(0), id2(0) {}
	ExtDataBlockInfo(quint8* a_data, int a_dataLen, int a_id1, int a_id2):
		data(a_data), dataLen(a_dataLen), id1(a_id1), id2(a_id2) {}
	quint8* data;
	int dataLen;
	int id1;
	int id2;
};

struct MPLSParser {
    enum DiskType {DT_NONE, DT_BLURAY, DT_AVCHD};
    static const int TIME_BASE = 45000;

	MPLSParser(): m_chapterLen(0), number_of_SubPaths(0), m_m2tsOffset(0) {}

	bool parse(const QString& fileName);
	void parse(quint8* buffer, int len);
	int compose(quint8* buffer, int bufferSize, DiskType dt);

	//char type_indicator[5];
	//char version_number[5];
	int PlayList_playback_type;
	int playback_count;
	//int number_of_PlayItems;
	int number_of_SubPaths;
	//char clip_Information_file_name[6];
	//char clip_codec_identifier[5];
	bool is_multi_angle;
	//int connection_condition;
	int ref_to_STC_id;
	bool PlayItem_random_access_flag;
	int number_of_angles;
	bool is_different_audios;
	bool is_seamless_angle_change;

	int m_chapterLen;

	quint32 IN_time;
	quint32 OUT_time;
	QVector<MPLSPlayItem> m_playItems;

	bool chapter_search_mask;
	bool time_search_mask;
	bool skip_to_next_point_mask;
	bool skip_back_to_previous_point_mask;
	bool stop_mask;
	bool pause_on_mask;
	bool still_off_mask;
	bool forward_play_mask;
	bool backward_play_mask;
	bool resume_mask;
	bool move_up_selected_button_mask;
	bool move_down_selected_button_mask;
	bool move_left_selected_button_mask;
	bool move_right_selected_button_mask;
	bool select_button_mask;
	bool activate_button_mask;
	bool select_button_and_activate_mask;
	bool primary_audio_stream_number_change_mask;
	bool angle_number_change_mask;
	bool popup_on_mask;
	bool popup_off_mask;
	bool PG_textST_enable_disable_mask;
	bool PG_textST_stream_number_change_mask;
	bool secondary_video_enable_disable_mask;
	bool secondary_video_stream_number_change_mask;
	bool secondary_audio_enable_disable_mask;
	bool secondary_audio_stream_number_change_mask;
	bool PiP_PG_textST_stream_number_change_mask;
	QVector<MPLSStreamInfo> m_streamInfo;
	QVector <PlayListMark> m_marks;
	void deserializePlayList(const QString& str);
	int m_m2tsOffset;
    QString fileExt;
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

class MovieObject {
public:
	bool parse(const char* fileName);
	void parse(quint8* buffer, int len);
    int compose(quint8* buffer, int len, MPLSParser::DiskType dt);
private:
	void parseMovieObjects(BitStreamReader& reader);
	void parseNavigationCommand(BitStreamReader& reader);
};

#endif
