#ifndef __AAC_CODEC_H
#define __AAC_CODEC_H

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QByteArray>

const static int AAC_HEADER_LEN = 7;

struct AdtsHeader
{
    uint8_t mpegVersion = 0;
    uint8_t protectionAbsent = 1;
    uint8_t profile = 0;
    uint8_t mpeg4SamplingFrequencyIndex = 0;
    uint8_t mpeg4ChannelConfiguration = 0;
    uint16_t frameLength = 0;
    uint16_t bufferFullness = 0;
    uint8_t numberOfAacFrames = 0;
    uint16_t crc = 0;

    static const int kMinLength = 7;
    static const int kMaxLength = 9;
    static const uint16_t kSyncWord = 0b111111111111;

    static const int kSyncWordLength = 12;
    static const int kMpegVersionLength = 1;
    static const int kLayerLength = 2;
    static const int kProtectionAbsentLength = 1;
    static const int kProfileLength = 2;
    static const int kMpeg4SamplingFrequencyIndexLength = 4;
    static const int kPrivateBitLength = 1;
    static const int kMpeg4ChannelConfigurationLength = 3;
    static const int kOriginalityLength = 1;
    static const int kHomeLength = 1;
    static const int kCopyrightedIdLength = 1;
    static const int kCopyrightIdStartLength = 1;
    static const int kFrameLengthLength = 13; //< Length of the frame in bytes, including header
    static const int kBufferFullnessLength = 11;
    static const int kNumberOfAacFramesLength = 2;
    static const int kCrcLength = 16;

    static const int kAscObjectTypeLength = 5;
    static const int kAscSamplingIndexLength = 4;
    static const int kAscChannelConfigLength = 4;
    static const int kAscFrameLengthLength = 1; //< 0 - 1024 samples per frame, 1 - 960.
    static const int kAscDependsOnCoreCoderLength = 1;
    static const int kAscIsExtensionLength = 1;

    static const int kAscLength = 2;

    bool decodeFromFrame(const uint8_t* const buffer, int bufferSize);
    bool encodeToFfmpegExtradata(QByteArray* outExtradata) const;
    int length() const;
};

class AACCodec {
public:
	// convert AAC prifle to mpeg 4 object type.
	//static const int object_type[9];
	const static int OBJECT_AAC_MAIN = 1;
	const static int OBJECT_AAC_LC = 2;
	const static int OBJECT_AAC_SSR = 3;
	const static int OBJECT_AAC_LTP = 4;

	const static int PROFILE_LC = 0;
	const static int PROFILE_MAIN = 1;
	const static int PROFILE_SSR = 2;
	const static int PROFILE_LTP = 3;

	static const int aac_sample_rates[16];
	static const int aac_channels[8];
	AACCodec() {
		m_sample_rate = 48000;
		m_channels = 0;
	}
	quint8* findAacFrame(quint8* buffer, quint8* end);
	int getFrameSize(quint8* buffer);
	bool decodeFrame(quint8* buffer, quint8* end);
	void buildADTSHeader(quint8* buffer, int frameSize);
	inline void setSampleRate(int value) {m_sample_rate = value;}
	bool readConfig(const QByteArray& data);
public:
    int m_id;
	int m_layer;
	int m_channels;
    int m_sample_rate;
    int m_samples;
    int m_bit_rate;
	int m_sample_rates_index;
	int m_channels_index;
	int m_profile;
    //int m_frameSize
	int m_rdb; // ch, sr;
};

#endif // ENABLE_DATA_PROVIDERS

#endif
