#include "aac.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/media/bitStream.h>
#include <utils/math/math.h>

// convert AAC prifle to mpeg 4 object type.

/*
const int AACCodec::object_type[9] = {1, // 'AAC_MAIN' -> AAC Main
								   2, // 'AAC_LC' -> AAC Low complexity
								   3, // 'AAC_SSR' -> AAC SSR
								   4, // 'AAC_LTP' -> AAC Long term prediction
								   5, // HE_AAC
								   17, // ER_LC   
								   19, // ER_LTP
								   23, // LD
								   27 //DRM_ER_LC
                                  };  
*/

bool AdtsHeader::decodeFromFrame(const uint8_t* const buffer, int bufferSize)
{
    if (bufferSize < kMinLength)
        return false;

    try
    {
        BitStreamReader reader(buffer, buffer + bufferSize);
        uint16_t syncWord = reader.getBits(kSyncWordLength);
        if (syncWord != kSyncWord)
            return false;

        mpegVersion = reader.getBits(kMpegVersionLength);

        reader.skipBits(kLayerLength);

        protectionAbsent = reader.getBits(kProtectionAbsentLength);
        profile = reader.getBits(kProfileLength);
        mpeg4SamplingFrequencyIndex = reader.getBits(kMpeg4SamplingFrequencyIndexLength);

        reader.skipBits(kPrivateBitLength);

        mpeg4ChannelConfiguration = reader.getBits(kMpeg4ChannelConfigurationLength);

        reader.skipBits(kOriginalityLength
            + kHomeLength
            + kCopyrightedIdLength
            + kCopyrightIdStartLength);

        frameLength = reader.getBits(kFrameLengthLength);
        bufferFullness = reader.getBits(kBufferFullnessLength);
        numberOfAacFrames = reader.getBits(kNumberOfAacFramesLength);

        if (!protectionAbsent)
            protectionAbsent = reader.getBits(kCrcLength);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool AdtsHeader::encodeToFfmpegExtradata(QByteArray* extradata) const
{
    extradata->resize(qPower2Ceil((unsigned int)kAscLength, sizeof(int)));
    auto buffer = extradata->data();
    try
    {
        BitStreamWriter writer;
        writer.setBuffer((quint8*)buffer, ((quint8*)buffer) + kAscLength);
        writer.putBits(kAscObjectTypeLength, profile);
        writer.putBits(kAscSamplingIndexLength, mpeg4SamplingFrequencyIndex);
        writer.putBits(kAscChannelConfigLength, mpeg4ChannelConfiguration);
        writer.putBit(0); //< Frame length - 1024 samples
        writer.putBit(0); //< Does not depend on core coder
        writer.putBit(0); //< Is not an extension

        writer.flushBits();
    }
    catch (...)
    {
        return false;
    }

    extradata->resize(kAscLength);
    return true;
}

int AdtsHeader::length() const
{
    return protectionAbsent ? kMinLength : kMaxLength;
}

const int AACCodec::aac_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

const int AACCodec::aac_channels[8] = {
    0, 1, 2, 3, 4, 5, 6, 8
};

quint8* AACCodec::findAacFrame(quint8* buffer, quint8* end)
{
	quint8* curBuf = buffer;
	while (curBuf < end) 
	{
		if (*curBuf < 0xf0)
			curBuf += 2;
		else if (*curBuf == 0xff && curBuf < end-1 && (curBuf[1]&0xf6) == 0xf0)
			return curBuf;
		else if ((*curBuf&0xf6) == 0xf0 && curBuf > buffer && curBuf[-1] == 0xff)
			return curBuf-1;
		else
			curBuf++;
	}
	return 0;
}

int AACCodec::getFrameSize(quint8* buffer)
{
	return buffer[4]*8 + (buffer[5] >> 5);
}

bool AACCodec::decodeFrame(quint8* buffer, quint8* end)
{
	BitStreamReader bits;
	try {
		bits.setBuffer(buffer, end);
		int synh = bits.getBits(12);

		if(synh != 0xfff)
			return false;
		m_id = bits.getBit();          /* 0: MPEG-4, 1: MPEG-2*/
		m_layer = bits.getBits(2);        /* layer */
		int protection_absent = bits.getBit();          /* protection_absent */
        (void) protection_absent;

		// -- 16 bit
		m_profile = bits.getBits(2);        /* profile_objecttype */
		m_sample_rates_index = bits.getBits(4);    /* sample_frequency_index */
		if(!aac_sample_rates[m_sample_rates_index])
			return false;
		bits.skipBit();          /* private_bit */
		m_channels_index = bits.getBits(3);    /* channel_configuration */
		if(!aac_channels[m_channels_index])
			return false;
		bits.skipBit();          /* original/copy */
		bits.skipBit();          /* home */
		//if (m_id == 0) // this is MPEG4
		//    bits.skipBits(2);   // emphasis      

		/* adts_variable_header */
		bits.skipBit();          /* copyright_identification_bit */
		bits.skipBit();          /* copyright_identification_start */
		// -- 32 bit getted
		int frameSize = bits.getBits(13) >> 2; /* aac_frame_length */
		//LTRACE(LT_DEBUG, 0, "decodec frame size: " << m_size);
		int adts_buffer_fullness = bits.getBits(11);       /* adts_buffer_fullness */
        (void) adts_buffer_fullness;

		m_rdb = bits.getBits(2);   /* number_of_raw_data_blocks_in_frame */

		m_channels = aac_channels[m_channels_index];
		m_sample_rate = aac_sample_rates[m_sample_rates_index];
		m_samples = (m_rdb + 1) * 1024;
		m_bit_rate = frameSize * 8 * m_sample_rate / m_samples; 
		return true;
	} catch(BitStreamException) {
		return false;
	}
}

void AACCodec::buildADTSHeader(quint8* buffer, int frameSize)
{
	BitStreamWriter writer;
	writer.setBuffer(buffer, buffer + AAC_HEADER_LEN);
	writer.putBits(12, 0xfff);
	writer.putBit(m_id);
	writer.putBits(2, m_layer);
	writer.putBit(1); // protection_absent
	writer.putBits(2, m_profile);
	m_sample_rates_index = 0;
	for (uint i = 0; i < sizeof(aac_sample_rates); i++)
		if (aac_sample_rates[i] == m_sample_rate) {
			m_sample_rates_index = i;
			break;
		}
	writer.putBits(4, m_sample_rates_index);
	writer.putBit(0); /* private_bit */
	m_channels_index = 0;
	for (uint i = 0; i < sizeof(aac_channels); i++)
		if (aac_channels[i] == m_channels) {
			m_channels_index = i;
			break;
		}
	writer.putBits(3, m_channels_index);

	writer.putBit(0); /* original/copy */
	writer.putBit(0); /* home */

	//if (m_id == 0) // this is MPEG4
	//    writer.putBits(2,0);   // emphasis      

    /* adts_variable_header */
    writer.putBit(0);          /* copyright_identification_bit */
    writer.putBit(0);          /* copyright_identification_start */

	writer.putBits(13,frameSize); // /* aac_frame_length */
	writer.putBits(11, 2047); // /* adts_buffer_fullness */
	writer.putBits(2, m_rdb); /* number_of_raw_data_blocks_in_frame */
	writer.flushBits();
}

bool AACCodec::readConfig(const QByteArray& data)
{
    try {
	    BitStreamReader reader((const quint8*) data.data(), (const quint8*) data.data() + data.size());
	    int object_type = reader.getBits(5);
	    if (object_type == 31)
		    object_type = 32 + reader.getBits(6);
	    m_profile = (object_type & 0x3)- 1;
        m_sample_rates_index = reader.getBits(4);
	    m_sample_rate = m_sample_rates_index == 0x0f ? reader.getBits(24) : aac_sample_rates[m_sample_rates_index];
	    m_channels_index = reader.getBits(4);
	    m_channels = aac_channels[m_channels_index];
        return true;
    } catch(...)
    {
        return false;
    }
}

#endif // ENABLE_DATA_PROVIDERS
