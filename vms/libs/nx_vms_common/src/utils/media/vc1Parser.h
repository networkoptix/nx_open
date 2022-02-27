// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef VC1_PARSER_H
#define VC1_PARSER_H

#include <QtCore/QString>

#include "bitStream.h"
#include <memory.h>

extern "C"
{
    #include <libavutil/avutil.h>
}

static const int NOT_ENOUGHT_BUFFER = -1;

enum VC1Code{
    VC1_CODE_ENDOFSEQ  = 0x0A,
    VC1_CODE_SLICE,
    VC1_CODE_FIELD,
    VC1_CODE_FRAME,
    VC1_CODE_ENTRYPOINT,
    VC1_CODE_SEQHDR,

    VC1_USER_CODE_SLICE = 0x1B,  // user-defined slice
    VC1_USER_CODE_FIELD,
    VC1_USER_CODE_FRAME,
    VC1_USER_CODE_ENTRYPOINT,
    VC1_USER_CODE_SEQHDR
};

/** Available Profiles */
//@{
enum Profile {
    PROFILE_SIMPLE,
    PROFILE_MAIN,
    PROFILE_COMPLEX, ///< TODO #rvasilenko WMV9 specific
    PROFILE_ADVANCED
};
//@}

const int ff_vc1_fps_nr[5] = { 24, 25, 30, 50, 60 };
const int ff_vc1_fps_dr[2] = { 1000, 1001 };

/*
struct AVRational {
    int w, h;
    AVRational() {w = h =0;}
    AVRational(int _w, int _h) {w = _w; h = _h;}
};
*/

const AVRational ff_vc1_pixel_aspect[16]={
    {0, 1},
    {1, 1},
    {12, 11},
    {10, 11},
    {16, 11},
    {40, 33},
    {24, 11},
    {20, 11},
    {32, 11},
    {80, 33},
    {18, 11},
    {15, 11},
    {64, 33},
    {160, 99},
    {0, 1},
    {0, 1}
};

enum VC1PictType {VC_I_TYPE, VC_P_TYPE, VC_B_TYPE, VC_BI_TYPE};
extern const char* pict_type_str[4];

class VC1Unit {
public:
    VC1Unit(): m_nalBuffer(0), m_nalBufferLen(0) {}
    ~VC1Unit() {delete m_nalBuffer;}

    static bool isMarker(quint8* ptr) { return ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1; }

    static quint8* findNextMarker(quint8* buffer, quint8* end)
    {
        for (buffer += 2; buffer < end;)
        {
            if (*buffer > 1)
                buffer += 3;
            else if (*buffer == 0)
                buffer ++;
            else if (buffer[-2] == 0 && buffer[-1] == 0) {
                return buffer-2;
            }
            else
                buffer+=3;
        }
        return end;
    }

    int vc1_unescape_buffer(const quint8 *src, int size)
    {
        if (m_nalBuffer != 0)
            delete m_nalBuffer;
        m_nalBuffer = new quint8[size];
        int dsize = 0, i;
        if(size < 4) {
            for(dsize = 0; dsize < size; dsize++) *m_nalBuffer++ = *src++;
            m_nalBufferLen = size;
            return size;
        }
        for(i = 0; i < size; i++, src++) {
            if(src[0] == 3 && i >= 2 && !src[-1] && !src[-2] && i < size-1 && src[1] < 4) {
                m_nalBuffer[dsize++] = src[1];
                src++;
                i++;
            } else
                m_nalBuffer[dsize++] = *src;
        }
        m_nalBufferLen = dsize;
        return dsize;
    }

    int vc1_escape_buffer(quint8 *dst)
    {

        quint8* srcStart = m_nalBuffer;
        quint8* initDstBuffer = dst;
        quint8* srcBuffer = m_nalBuffer;
        quint8* srcEnd = m_nalBuffer + m_nalBufferLen;
        for (srcBuffer += 2; srcBuffer < srcEnd;)
        {
            if (*srcBuffer > 3)
                srcBuffer += 3;
            else if (srcBuffer[-2] == 0 && srcBuffer[-1] == 0) {
                memcpy(dst, srcStart, srcBuffer - srcStart);
                dst += srcBuffer - srcStart;
                *dst++ = 3;
                *dst++ = *srcBuffer++;
                for (int k = 0; k < 1; k++)
                    if (srcBuffer < srcEnd) {
                        *dst++ = *srcBuffer++;
                    }
                srcStart = srcBuffer;
            }
            else
                srcBuffer++;
        }
        memcpy(dst, srcStart, srcEnd - srcStart);
        dst += srcEnd - srcStart;
        return (int) (dst - initDstBuffer);
    }

    const BitStreamReader& getBitReader() {return bitReader;}

protected:
    void updateBits(int bitOffset, int bitLen, int value);
    BitStreamReader bitReader;
    quint8* m_nalBuffer;
    uint32_t m_nalBufferLen;
};

class VC1SequenceHeader: public VC1Unit {
public:
    VC1SequenceHeader(): VC1Unit(), interlace(0), time_base_num(0), time_base_den(0), m_fpsFieldBitVal(0)
    {
        sample_aspect_ratio.num = sample_aspect_ratio.den = 1;
    }
    int profile;
    int res_sm;
    int frmrtq_postproc;
    int bitrtq_postproc;
    int loop_filter;
    int multires;
    int fastuvmc;
    int extended_mv;
    int dquant;
    int vstransform;
    int overlap;
    int resync_marker;
    int rangered;
    int max_b_frames;
    int quantizer_mode;
        int finterpflag;      ///< INTERPFRM present
    int level;
    int chromaformat;     ///< 2bits, 2=4:2:0, only defined
    int coded_width;
    int coded_height;
    int display_width;
    int display_height;
    int pulldown;        ///< TFF/RFF present
    bool interlace;        ///< Progressive/interlaced (RPTFTM syntax element)
    int tfcntrflag;       ///< TFCNTR present
        int psf;              ///< Progressive Segmented Frame
    int time_base_num;
    int time_base_den;
    int color_prim;       ///< 8bits, chroma coordinates of the color primaries
        int transfer_char;    ///< 8bits, Opto-electronic transfer characteristics
    int matrix_coef;      ///< 8bits, Color primaries->YCbCr transform matrix
    int hrd_param_flag;
        int hrd_num_leaky_buckets;
    int postprocflag;

/* for decoding entry point */
    int panscanflag;
    int decode_entry_point();
    int extended_dmv;
/* ------------------------*/

    AVRational sample_aspect_ratio; // w, h
    int decode_sequence_header();
    int decode_sequence_header_adv();
    QString getStreamDescr();
    double getFPS();
    bool setFPS(double value);
private:
    int m_fpsFieldBitVal;
};

/*
VC1EntryPoint: public VC1Unit {
public:
    int panscanflag;
    int loop_filter;
    int fastuvmc;
    int extended_mv;
    int dquant;
    int vstransform;
    int overlap;
    int quantizer_mode;
    int coded_width;
    int coded_height;
    int extended_dmv;     ///< Additional extended dmv range at P/B frame-level
};
*/

class VC1Frame: public VC1Unit {
public:
    VC1Frame(): VC1Unit(), fcm(0), rptfrm(0), rff(0), rptfrmBitPos(0) {}
    int fcm;
    int interpfrm;
    int framecnt;
    int rangeredfrm;
    int pict_type;
    int rptfrm;
    int tff;
    int rff;
    int rptfrmBitPos;
    int decode_frame_direct(const VC1SequenceHeader& sequenceHdr, const quint8* buffer, const quint8* end);
private:
    int vc1_parse_frame_header(const VC1SequenceHeader& sequenceHdr);
    int vc1_parse_frame_header_adv(const VC1SequenceHeader& sequenceHdr);
};

#endif
