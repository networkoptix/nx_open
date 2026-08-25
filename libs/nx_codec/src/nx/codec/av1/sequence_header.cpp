// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sequence_header.h"

#include <nx/utils/bit_stream.h>

namespace nx::media::av1 {

namespace {

/** uvlc(), see the AV1 spec, 4.10.3. */
uint32_t readUvlc(nx::utils::BitStreamReader& reader)
{
    // The terminating bit is consumed even when there are more than 32 leading zeros, otherwise
    // the reader would be left one bit short and everything after it would be misparsed. The loop
    // is bounded by the data: getBit() throws BitStreamException once the buffer is exhausted.
    int leadingZeros = 0;
    while (!reader.getBit())
        ++leadingZeros;

    if (leadingZeros >= 32)
        return UINT32_MAX;

    return reader.getBits(leadingZeros) + (1u << leadingZeros) - 1;
}

/** timing_info(), see the AV1 spec, 5.5.3. */
void readTimingInfo(nx::utils::BitStreamReader& reader)
{
    reader.skipBits(32); //< num_units_in_display_tick
    reader.skipBits(32); //< time_scale
    const bool equalPictureInterval = reader.getBit();
    if (equalPictureInterval)
        readUvlc(reader); //< num_ticks_per_picture_minus_1
}

/** decoder_model_info(), see the AV1 spec, 5.5.4. @return buffer_delay_length_minus_1. */
int readDecoderModelInfo(nx::utils::BitStreamReader& reader)
{
    const int bufferDelayLengthMinus1 = reader.getBits(5);
    reader.skipBits(32); //< num_units_in_decoding_tick
    reader.skipBits(5); //< buffer_removal_time_length_minus_1
    reader.skipBits(5); //< frame_presentation_time_length_minus_1
    return bufferDelayLengthMinus1;
}

} // namespace

bool SequenceHeader::read(const uint8_t* data, int size)
{
    try
    {
        nx::utils::BitStreamReader reader(data, size);

        seqProfile = reader.getBits(3);
        stillPicture = reader.getBit();
        reducedStillPictureHeader = reader.getBit();

        if (reducedStillPictureHeader)
        {
            seqLevelIdx0 = reader.getBits(5);
            seqTier0 = 0;
        }
        else
        {
            bool decoderModelInfoPresentFlag = false;
            int bufferDelayLengthMinus1 = 0;
            if (reader.getBit()) //< timing_info_present_flag
            {
                readTimingInfo(reader);
                decoderModelInfoPresentFlag = reader.getBit();
                if (decoderModelInfoPresentFlag)
                    bufferDelayLengthMinus1 = readDecoderModelInfo(reader);
            }

            const bool initialDisplayDelayPresentFlag = reader.getBit();
            const int operatingPointsCntMinus1 = reader.getBits(5);
            for (int i = 0; i <= operatingPointsCntMinus1; ++i)
            {
                reader.skipBits(12); //< operating_point_idc[i]
                const uint8_t seqLevelIdx = reader.getBits(5);
                uint8_t seqTier = 0;
                if (seqLevelIdx > 7)
                    seqTier = reader.getBit();

                if (decoderModelInfoPresentFlag)
                {
                    if (reader.getBit()) //< decoder_model_present_for_this_op[i]
                    {
                        // operating_parameters_info(i), see the AV1 spec, 5.5.5.
                        reader.skipBits(bufferDelayLengthMinus1 + 1); //< decoder_buffer_delay[i]
                        reader.skipBits(bufferDelayLengthMinus1 + 1); //< encoder_buffer_delay[i]
                        reader.skipBit(); //< low_delay_mode_flag[i]
                    }
                }

                if (initialDisplayDelayPresentFlag)
                {
                    if (reader.getBit()) //< initial_display_delay_present_for_this_op[i]
                        reader.skipBits(4); //< initial_display_delay_minus_1[i]
                }

                if (i == 0)
                {
                    seqLevelIdx0 = seqLevelIdx;
                    seqTier0 = seqTier;
                }
            }
        }

        const int frameWidthBitsMinus1 = reader.getBits(4);
        const int frameHeightBitsMinus1 = reader.getBits(4);
        maxFrameWidth = reader.getBits(frameWidthBitsMinus1 + 1) + 1;
        maxFrameHeight = reader.getBits(frameHeightBitsMinus1 + 1) + 1;
        frameIdNumbersPresentFlag = reducedStillPictureHeader ? false : (bool) reader.getBit();
        return true;
    }
    catch (const nx::utils::BitStreamException&)
    {
        return false;
    }
}

} // namespace nx::media::av1
