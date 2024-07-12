// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

#include <nx/utils/bit_stream.h>

namespace nx::media::h264 {

enum NALUnitType
{
    nuUnspecified, nuSliceNonIDR, nuSliceA, nuSliceB, nuSliceC,  // 0..4
    nuSliceIDR, nuSEI, nuSPS, nuPPS, nuDelimiter,                // 5..9
    nuEOSeq, nuEOStream, nuFillerData, nuSPSExt, nuReserved1,    // 10..14
    nuReserved2, nuReserved3, nuReserved4, nuReserved5, nuSliceWithoutPartitioning,  // 15..19
    nuReserved6, nuReserved7, nuReserved8, nuReserved9, // 20..23
    STAP_A_PACKET, STAP_B_PACKET, MTAP16_PACKET, MTAP24_PACKET, FU_A_PACKET, FU_B_PACKET,
    nuDummy
};

class NX_CODEC_API NALUnit
{
public:
    NALUnit() {}
    NALUnit(const NALUnit&) = delete;
    NALUnit& operator=(const NALUnit& right) = delete;

public:
    static constexpr uint8_t kStartCode[] = {0x00, 0x00, 0x01};
    static constexpr uint8_t kStartCodeLong[] = {0x00, 0x00, 0x00, 0x01};

    const static int SPS_OR_PPS_NOT_READY = 1;
    const static int NOT_ENOUGHT_BUFFER = 2;
    const static int NOT_FOUND = 4;
    const static int UNIT_SKIPPED = 5;
    int nal_ref_idc;
    int nal_unit_type;

    uint8_t* m_nalBuffer = nullptr;
    int m_nalBufferLen = 0;

    virtual ~NALUnit() {
        delete [] m_nalBuffer;
    }
    static bool isSliceNal(uint8_t nalUnitType);
    int deserialize(uint8_t* buffer, uint8_t* end);
    virtual int serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd, bool writeStartCode) const;
    virtual int serialize(uint8_t* dstBuffer);
    //void setBuffer(uint8_t* buffer, uint8_t* end);
    void decodeBuffer(const uint8_t* buffer,const uint8_t* end);

    const nx::utils::BitStreamReader& getBitReader() const {return bitReader;}

protected:
    nx::utils::BitStreamReader bitReader;
    void updateBits(int bitOffset, int bitLen, int value);
    void scaling_list(int* scalingList, int sizeOfScalingList, bool& useDefaultScalingMatrixFlag);
};

NX_CODEC_API NALUnitType decodeType(uint8_t data);
NX_CODEC_API bool isSliceNal(uint8_t nalUnitType);
NX_CODEC_API bool isIFrame(const uint8_t* data, int dataLen);
NX_CODEC_API int ceil_log2(double val);

} // namespace nx::media::h264
