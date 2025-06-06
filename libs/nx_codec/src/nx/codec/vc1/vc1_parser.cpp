// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vc1_parser.h"

#include <memory>
#include <sstream>

#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

const char* pict_type_str[4] = {"I_TYPE", "P_TYPE", "B_TYPE", "BI_TYPE"};

static inline int get_unary(nx::utils::BitStreamReader& bitReader, int stop, int len)
{
    int i;
    for(i = 0; i < len && bitReader.getBit() != (uint)stop; i++);
    return i;
}

static inline int decode012(nx::utils::BitStreamReader& bitReader){
    int n = bitReader.getBit();
    if (n == 0)
        return 0;
    else
        return bitReader.getBit() + 1;
}

// ---------------------------- VC1Unit ------------------------------------------

void VC1Unit::updateBits(int bitOffset, int bitLen, int value)
{
    quint8* ptr = (quint8*) bitReader.getBuffer() + bitOffset/8;
    nx::utils::BitStreamWriter bitWriter;
    int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    quint8* ptr_end = (quint8*) bitReader.getBuffer() + (bitOffset + bitLen)/8;
    int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0) {
        int prefix = *ptr >> (8-byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8) {
        int postfix = *ptr_end & (( 1 << endBitsPostfix)-1);
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}

// ---------------------------- VC1SequenceHeader ------------------------------------------

QString VC1SequenceHeader::getStreamDescr()
{
    QString tmpStr;
    QTextStream rez(&tmpStr);
    rez << "Profile: ";
    switch (profile) {
        case PROFILE_SIMPLE:
            rez << "Simple";
            break;
        case PROFILE_MAIN:
            rez << "Main";
            break;
        case PROFILE_COMPLEX:
            rez << "Complex";
            break;
        case PROFILE_ADVANCED:
            rez << "Advanced@" << level;
            break;
        default:
            rez << "Unknown";
            break;
    };
    rez << " Resolution: " << coded_width << ':' << coded_height;
    rez << (interlace ? 'i' : 'p') << "  ";
    rez << "Frame rate: ";
    double fps = getFPS();
    if (fps != 0)
        rez <<  fps;
    else
        rez << "not found";
    return tmpStr;
}

double VC1SequenceHeader::getFPS() {
    if (time_base_num == 0 || time_base_den == 0)
        return 0;
    double fps = time_base_den / (double) time_base_num;
    //if (fps > 25.0 && pulldown)
    //    fps /= 1.25;
    return fps;
}

bool VC1SequenceHeader::setFPS(double value)
{
    //if (value < 25.0 && pulldown)
    //    value *= 1.25;
    int nr, dr;
    if (m_fpsFieldBitVal > 0) {
        int time_scale = (uint32_t)(value +0.5)  * 1000;
        int num_units_in_tick = time_scale / value + 0.5;
        if ((time_scale == 24'000 || time_scale == 25'000 || time_scale == 30'000 || time_scale == 50'000 || time_scale == 60'000)
             && (num_units_in_tick == 1000 || num_units_in_tick == 1001)) {
            time_base_den = time_scale;
            time_base_num = num_units_in_tick;
        }
        else {
            NX_DEBUG(this, "Can't overwrite stream fps. Non standard fps values not supported for VC-1 streams");
            return false;
        }
        if (time_scale == 24'000)
            nr = 1;
        else if (time_scale == 25'000)
            nr = 2;
        else if (time_scale == 30'000)
            nr = 3;
        else if (time_scale == 50'000)
            nr = 4;
        else //if (time_scale == 60'000)
            nr = 5;
        if (num_units_in_tick == 1000)
            dr = 1;
        else
            dr = 2;
        updateBits(m_fpsFieldBitVal, 8, nr);
        updateBits(m_fpsFieldBitVal + 8, 4, dr);
        return true;
    }
    else {
        NX_DEBUG(this, "Can't overwrite stream fps. Non standard fps values not supported for VC-1 streams");
        return false;
    }
}

int VC1SequenceHeader::decode_sequence_header()
{
    try {
        bitReader.setBuffer(m_nalBuffer, m_nalBuffer + m_nalBufferLen); // skip 00 00 01 xx marker
        profile = bitReader.getBits(2);
        if (profile == PROFILE_COMPLEX)
            NX_DEBUG(this, "WMV3 Complex Profile is not fully supported");

        if (profile == PROFILE_ADVANCED)
            return decode_sequence_header_adv();
        else
        {
            res_sm = bitReader.getBits(2); //reserved
            if (res_sm) {
                NX_DEBUG(this, "Reserved RES_SM=%1 is forbidden", res_sm);
                return UNSUPPORTED_PARAM;
            }
        }

        frmrtq_postproc = bitReader.getBits(3); //common
        bitrtq_postproc = bitReader.getBits(5); //common
        loop_filter = bitReader.getBit(); //common
        if(loop_filter == 1 && profile == PROFILE_SIMPLE)
            NX_DEBUG(this, "LOOPFILTER shell not be enabled in simple profile");
        int res_x8 = bitReader.getBit(); //reserved
        if (res_x8)
            NX_DEBUG(this, "1 for reserved RES_X8 is forbidden");
        multires = bitReader.getBit();
        int res_fasttx = bitReader.getBit();
        if (!res_fasttx)
            NX_DEBUG(this, "0 for reserved RES_FASTTX is forbidden");
        fastuvmc = bitReader.getBit();
        if (!profile && !fastuvmc) {
            NX_DEBUG(this, "FASTUVMC unavailable in Simple Profile");
            return UNSUPPORTED_PARAM;
        }
        extended_mv = bitReader.getBit();
        if (!profile && extended_mv) {
            NX_DEBUG(this, "Extended MVs unavailable in Simple Profile");
            return UNSUPPORTED_PARAM;
        }
        dquant = bitReader.getBits(2);
        vstransform = bitReader.getBit();
        int res_transtab = bitReader.getBit();
        if (res_transtab) {
            NX_DEBUG(this, "1 for reserved RES_TRANSTAB is forbidden");
            return UNSUPPORTED_PARAM;
        }
        overlap = bitReader.getBit();
        resync_marker = bitReader.getBit();
        rangered = bitReader.getBit();
        if (rangered && profile == PROFILE_SIMPLE)
            NX_DEBUG(this, "RANGERED should be set to 0 in simple profile");
        max_b_frames = bitReader.getBits(3);
        quantizer_mode = bitReader.getBits(2);
        finterpflag = bitReader.getBit();
        int res_rtm_flag = bitReader.getBit();
        if (res_rtm_flag)
            NX_DEBUG(this, "Old WMV3 version detected.");
        // TODO: figure out what they mean (always 0x402F)
        if(!res_fasttx) bitReader.skipBits(16);
        return 0;
    } catch (const nx::utils::BitStreamException&) {
        return NOT_ENOUGHT_BUFFER;
    }
}

int VC1SequenceHeader::decode_sequence_header_adv()
{
    level = bitReader.getBits(3);
    if(level >= 5)
        NX_DEBUG(this, "Reserved LEVEL %1", level);
    chromaformat = bitReader.getBits(2);
    frmrtq_postproc = bitReader.getBits(3); //common
    bitrtq_postproc = bitReader.getBits(5); //common
    postprocflag = bitReader.getBit(); //common
    coded_width = (bitReader.getBits(12) + 1) << 1;
    coded_height = (bitReader.getBits(12) + 1) << 1;
    pulldown = bitReader.getBit();
    interlace = bitReader.getBit();
    tfcntrflag = bitReader.getBit();
    finterpflag = bitReader.getBit();
    bitReader.skipBit();
    psf = bitReader.getBit();
    /*
    if(psf) { //PsF, 6.1.13
        qWarning() << "Progressive Segmented Frame mode: not supported (yet)";
        return UNSUPPORTED_PARAM;
    }
    */
    max_b_frames = 7;
    if(bitReader.getBit()) { //Display Info - decoding is not affected by it
        int w, h, ar = 0;
        display_width  = w = bitReader.getBits(14) + 1;
        display_height = h = bitReader.getBits(14) + 1;
        if(bitReader.getBit())
            ar = bitReader.getBits(4);
        if(ar && ar < 14){
            sample_aspect_ratio = ff_vc1_pixel_aspect[ar];
        }else if(ar == 15){
            w = bitReader.getBits(8);
            h = bitReader.getBits(8);
            sample_aspect_ratio.num = w;
            sample_aspect_ratio.den = h;
        }

        if(bitReader.getBit()){ //framerate stuff
            if(bitReader.getBit()) {
                time_base_num = 32;
                time_base_den = bitReader.getBits(16) + 1;
            } else {
                int nr, dr;
                m_fpsFieldBitVal = bitReader.getBitsCount();
                nr = bitReader.getBits(8);
                dr = bitReader.getBits(4);
                if(nr && nr < 8 && dr && dr < 3){
                    time_base_num = ff_vc1_fps_dr[dr - 1];
                    time_base_den = ff_vc1_fps_nr[nr - 1] * 1000;
                }
                else
                    NX_DEBUG(this, "Invalid fps value");
            }
        }

        if(bitReader.getBit()){
            color_prim = bitReader.getBits(8);
            transfer_char = bitReader.getBits(8);
            matrix_coef = bitReader.getBits(8);
        }
    }

    hrd_param_flag = bitReader.getBit();
    if(hrd_param_flag) {
        hrd_num_leaky_buckets = bitReader.getBits(5);
        bitReader.skipBits(4); //bitrate exponent
        bitReader.skipBits(4); //buffer size exponent
        for(int i = 0; i < hrd_num_leaky_buckets; i++) {
            bitReader.skipBits(16); //hrd_rate[n]
            bitReader.skipBits(16); //hrd_buffer[n]
        }
    }
    return 0;
}

int VC1SequenceHeader::decode_entry_point()
{
    int i;
    try {
        bitReader.setBuffer(m_nalBuffer, m_nalBuffer + m_nalBufferLen); // skip 00 00 01 xx marker
        //blink = bitReader.getBit(); // broken link
        //clentry = bitReader.getBit(); // closed entry
        bitReader.skipBits(2);
        panscanflag = bitReader.getBit();
        //refdist = bitReader.getBit(); // refdist flag
        bitReader.skipBit();
        loop_filter = bitReader.getBit();
        fastuvmc = bitReader.getBit();
        extended_mv = bitReader.getBit();
        dquant = bitReader.getBits(2);
        vstransform = bitReader.getBit();
        overlap = bitReader.getBit();
        quantizer_mode = bitReader.getBits(2);

        if(hrd_param_flag) {
            for(i = 0; i < hrd_num_leaky_buckets; i++)
                bitReader.skipBits(8); //hrd_full[n]
        }

        if(bitReader.getBit()) {
            coded_width = (bitReader.getBits(12)+1)<<1;
            coded_height = (bitReader.getBits(12)+1)<<1;
        }
        if(extended_mv)
            extended_dmv = bitReader.getBit();
        if(bitReader.getBit()) {
            //av_log(avctx, AV_LOG_ERROR, "Luma scaling is not supported, expect wrong picture\n");
            bitReader.skipBits(3); // Y range, ignored for now
        }
        if(bitReader.getBit()) {
            //av_log(avctx, AV_LOG_ERROR, "Chroma scaling is not supported, expect wrong picture\n");
            bitReader.skipBits(3); // UV range, ignored for now
        }
        return 0;
    } catch (const nx::utils::BitStreamException&) {
        return NOT_ENOUGHT_BUFFER;
    }
}

// -------------------------- VC1Frame ---------------------------

int VC1Frame::decode_frame_direct(const VC1SequenceHeader& sequenceHdr, const quint8* buffer, const quint8* end)
{
    try {
        bitReader.setBuffer(buffer, end); // skip 00 00 01 xx marker
        if (sequenceHdr.profile < PROFILE_ADVANCED)
            return vc1_parse_frame_header(sequenceHdr);
        else
            return vc1_parse_frame_header_adv(sequenceHdr);
    } catch (const nx::utils::BitStreamException&) {
        return NOT_ENOUGHT_BUFFER;
    }
}

int VC1Frame::vc1_parse_frame_header(const VC1SequenceHeader& sequenceHdr)
{
    interpfrm = -1;
    if(sequenceHdr.finterpflag)
        interpfrm = bitReader.getBit();
    framecnt = bitReader.getBits(2); //framecnt unused
    rangeredfrm = 0;
    if (sequenceHdr.rangered)
        rangeredfrm = bitReader.getBit();
    pict_type = bitReader.getBit();
    if (sequenceHdr.max_b_frames > 0) {
        if (pict_type == 0) {
            if (bitReader.getBit()) pict_type = VC_I_TYPE;
            else pict_type = VC_B_TYPE;
        } else pict_type = VC_P_TYPE;
    }
    else
        pict_type = pict_type ? VC_P_TYPE : VC_I_TYPE;
    return 0;
}

int VC1Frame::vc1_parse_frame_header_adv(const VC1SequenceHeader& sequenceHdr)
{
    fcm = 0;
    if(sequenceHdr.interlace)
        fcm = decode012(bitReader);

    /*
    if (fcm == 2) // is coded field
    {
        int tmpType = bitReader.getBits(3);
        switch(tmpType) {
            case 0:
                pict_type = I_TYPE;
                break;
            case 1:
                pict_type = isFrame ? I_TYPE : P_TYPE;
                break;
            case 2:
                pict_type = isFrame ? P_TYPE : I_TYPE;
                break;
            case 3:
                pict_type = P_TYPE;
                break;
            case 4:
                pict_type = B_TYPE;
                break;
            case 5:
                pict_type = isFrame ? B_TYPE : BI_TYPE;
                break;
            case 6:
                pict_type = isFrame ? BI_TYPE : B_TYPE;
                break;
            case 7:
                pict_type = isFrame ? BI_TYPE : B_TYPE;
                break;
        }
    }
    */
    if (fcm == 2) // is coded field
    {
        switch(bitReader.getBits(3)) {
            case 0:
            case 1:
                pict_type = VC_I_TYPE;
                break;
            case 2:
            case 3:
                pict_type = VC_P_TYPE;
                break;
            case 4:
            case 5:
                pict_type = VC_B_TYPE;
                break;
            case 6:
            case 7:
                pict_type = VC_BI_TYPE;
                break;
        }
    }
    else {
        switch(get_unary(bitReader, 0, 4)) {
        case 0:
            pict_type = VC_P_TYPE;
            break;
        case 1:
            pict_type = VC_B_TYPE;
            break;
        case 2:
            pict_type = VC_I_TYPE;
            break;
        case 3:
            pict_type = VC_BI_TYPE;
            break;
        case 4:
            pict_type = VC_P_TYPE; // skipped pic
            break;
        }
    }

    if(sequenceHdr.tfcntrflag)
        //int TFCNTR = bitReader.getBits(8);
        bitReader.skipBits(8);
    if(sequenceHdr.pulldown) {
        rptfrmBitPos = bitReader.getBitsCount();
        if(!sequenceHdr.interlace || sequenceHdr.psf) {
            rptfrm = bitReader.getBits(2); // Repeat Frame Count (0 .. 3)
        } else {
            tff = bitReader.getBit();    // TFF (top field first)
            rff = bitReader.getBit(); // RFF (repeat first field)
        }
    }
    return 0;
}
