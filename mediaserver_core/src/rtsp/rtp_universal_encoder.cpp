#include "rtp_universal_encoder.h"
#include <nx/streaming/rtp_stream_parser.h>
#include <nx/streaming/config.h>
#include "common/common_module.h"

#include <core/resource/param.h>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/base64.h>
#include <libavutil/avstring.h>
}

static const int  RTP_PT_PRIVATE = 96;

/* from http://www.iana.org/assignments/rtp-parameters last updated 05 January 2005 */
/* payload types >= 96 are dynamic;
 * payload types between 72 and 76 are reserved for RTCP conflict avoidance;
 * all the other payload types not present in the table are unassigned or
 * reserved
 */
static const struct
{
    int pt;
    const char enc_name[14];
    enum AVMediaType codec_type;
    enum AVCodecID codec_id;
    int clock_rate;
    int audio_channels;
} AVRtpPayloadTypes[]=
{
  {0, "PCMU",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_MULAW, 8000, 1},
  {3, "GSM",         AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
  {4, "G723",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_G723_1, 8000, 1},
  {5, "DVI4",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
  {6, "DVI4",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 16000, 1},
  {7, "LPC",         AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
  {8, "PCMA",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_ALAW, 8000, 1},
  {9, "G722",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_ADPCM_G722, 8000, 1},
  {10, "L16",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_S16BE, 44100, 2},
  {11, "L16",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_PCM_S16BE, 44100, 1},
  {12, "QCELP",      AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_QCELP, 8000, 1},
  {13, "CN",         AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
  {14, "MPA",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_MP2, -1, -1},
  {97, "mpa-robust",        AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_MP3, -1, -1},
  {15, "G728",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
  {16, "DVI4",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 11025, 1},
  {17, "DVI4",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 22050, 1},
  {18, "G729",       AVMEDIA_TYPE_AUDIO,   AV_CODEC_ID_NONE, 8000, 1},
  {25, "CelB",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_NONE, 90000, -1},
  {26, "JPEG",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MJPEG, 90000, -1},
  {28, "nv",         AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_NONE, 90000, -1},
  {31, "H261",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_H261, 90000, -1},
  {32, "MPV",        AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MPEG1VIDEO, 90000, -1},
  {32, "MPV",        AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MPEG2VIDEO, 90000, -1},
  {33, "MP2T",       AVMEDIA_TYPE_DATA,    AV_CODEC_ID_MPEG2TS, 90000, -1},
  {34, "H263",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_H263, 90000, -1},
  {96, "H264",       AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_H264, 90000, -1},
  {96, "MPEG4-GENERIC",  AVMEDIA_TYPE_VIDEO,   AV_CODEC_ID_MPEG4, 90000, -1},
  {-1, "",           AVMEDIA_TYPE_UNKNOWN, AV_CODEC_ID_NONE, -1, -1}
};

#define MAX_PSET_SIZE 1024
#define MAX_EXTRADATA_SIZE ((INT_MAX - 10) / 2)
#define RTP_XIPH_IDENT 0xfecdba

static char *ff_data_to_hex(char *buff, const uint8_t *src, int s, int lowercase)
{
    int i;
    static const char hex_table_uc[16] = { '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F' };
    static const char hex_table_lc[16] = { '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f' };
    const char *hex_table = lowercase ? hex_table_lc : hex_table_uc;

    for(i = 0; i < s; i++) {
        buff[i * 2]     = hex_table[src[i] >> 4];
        buff[i * 2 + 1] = hex_table[src[i] & 0xF];
    }

    return buff;
}

static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t*)p;
        //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
        //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
        if ((x - 0x01010101) & (~x) & 0x80808080) { // generic
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

static const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end){
    const uint8_t *out= ff_avc_find_startcode_internal(p, end);
    if(p<out && out<end && !out[-1]) out--;
    return out;
}

static char *extradata2psets(AVCodecContext *c)
{
    char *psets, *p;
    const uint8_t *r;
    const char *pset_string = "; sprop-parameter-sets=";
    uint8_t *orig_extradata = NULL;
    int orig_extradata_size = 0;

    if (c->extradata_size > MAX_EXTRADATA_SIZE) {
        qWarning() << "Too much extradata!";

        return NULL;
    }
    if (c->extradata[0] == 1) {
        uint8_t *dummy_p;
        int dummy_int;
        AVBitStreamFilterContext *bsfc= av_bitstream_filter_init("h264_mp4toannexb");

        if (!bsfc) {
            qWarning() << "Cannot open the h264_mp4toannexb BSF!";

            return NULL;
        }

        orig_extradata_size = c->extradata_size;
        orig_extradata = (uint8_t*) av_mallocz(orig_extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
        if (!orig_extradata) {
            av_bitstream_filter_close(bsfc);
            return NULL;
        }
        memcpy(orig_extradata, c->extradata, orig_extradata_size);
        av_bitstream_filter_filter(bsfc, c, NULL, &dummy_p, &dummy_int, NULL, 0, 0);
        av_bitstream_filter_close(bsfc);
    }

    psets = (char*) av_mallocz(MAX_PSET_SIZE);
    if (psets == NULL) {
        qWarning() << "Cannot allocate memory for the parameter sets.";
        av_free(orig_extradata);
        return NULL;
    }
    memcpy(psets, pset_string, strlen(pset_string));
    p = psets + strlen(pset_string);
    r = ff_avc_find_startcode(c->extradata, c->extradata + c->extradata_size);
    while (r < c->extradata + c->extradata_size) {
        const uint8_t *r1;
        uint8_t nal_type;

        while (!*(r++));
        nal_type = *r & 0x1f;
        r1 = ff_avc_find_startcode(r, c->extradata + c->extradata_size);
        if (nal_type != 7 && nal_type != 8) { /* Only output SPS and PPS */
            r = r1;
            continue;
        }
        if (p != (psets + strlen(pset_string))) {
            *p = ',';
            p++;
        }
        if (av_base64_encode(p, MAX_PSET_SIZE - (p - psets), r, r1 - r) == NULL) {
            qWarning() << "Cannot Base64-encode" << MAX_PSET_SIZE - (p - psets) << r1 - r;
            av_free(psets);

            return NULL;
        }
        p += strlen(p);
        r = r1;
    }
    if (orig_extradata) {
        av_free(c->extradata);
        c->extradata      = orig_extradata;
        c->extradata_size = orig_extradata_size;
    }

    return psets;
}

static char *extradata2config(AVCodecContext *c)
{
    char *config;

    if (c->extradata_size > MAX_EXTRADATA_SIZE) {
        qWarning() << "Too much extradata!";

        return NULL;
    }
    config = (char*) av_malloc(10 + c->extradata_size * 2);
    if (config == NULL) {
        qWarning() << "Cannot allocate memory for the config info.";
        return NULL;
    }
    memcpy(config, "; config=", 9);
    ff_data_to_hex(config + 9, c->extradata, c->extradata_size, 0);
    config[9 + c->extradata_size * 2] = 0;

    return config;
}

static const int avpriv_mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static char *latm_context2config(AVCodecContext *c)
{
    /* MP4A-LATM
     * The RTP payload format specification is described in RFC 3016
     * The encoding specifications are provided in ISO/IEC 14496-3 */

    uint8_t config_byte[6];
    int rate_index;
    char *config;

    for (rate_index = 0; rate_index < 16; rate_index++)
        if (avpriv_mpeg4audio_sample_rates[rate_index] == c->sample_rate)
            break;
    if (rate_index == 16) {
        qWarning() << "Unsupported sample rate";
        return NULL;
    }

    config_byte[0] = 0x40;
    config_byte[1] = 0;
    config_byte[2] = 0x20 | rate_index;
    config_byte[3] = c->channels << 4;
    config_byte[4] = 0x3f;
    config_byte[5] = 0xc0;

    config = (char*) av_malloc(6*2+1);
    if (!config) {
        qWarning() << "Cannot allocate memory for the config info.";
        return NULL;
    }
    ff_data_to_hex(config, config_byte, 6, 1);
    config[12] = 0;

    return config;
}

static int latm_context2profilelevel(AVCodecContext *c)
{
    /* MP4A-LATM
     * The RTP payload format specification is described in RFC 3016
     * The encoding specifications are provided in ISO/IEC 14496-3 */

    int profile_level = 0x2B;

    /* TODO: AAC Profile only supports AAC LC Object Type.
     * Different Object Types should implement different Profile Levels */

    if (c->sample_rate <= 24000) {
        if (c->channels <= 2)
            profile_level = 0x28; // AAC Profile, Level 1
    } else if (c->sample_rate <= 48000) {
        if (c->channels <= 2) {
            profile_level = 0x29; // AAC Profile, Level 2
        } else if (c->channels <= 5) {
            profile_level = 0x2A; // AAC Profile, Level 4
        }
    } else if (c->sample_rate <= 96000) {
        if (c->channels <= 5) {
            profile_level = 0x2B; // AAC Profile, Level 5
        }
    }

    return profile_level;
}

quint16 AV_RB16(quint8* data)
{
    return (data[0] << 8) + data[1];
}

static int avpriv_split_xiph_headers(uint8_t *extradata, int extradata_size,
                              int first_header_size, uint8_t *header_start[3],
                              int header_len[3])
{
    int i;

    if (extradata_size >= 6 && AV_RB16(extradata) == first_header_size) {
        int overall_len = 6;
        for (i=0; i<3; i++) {
            header_len[i] = AV_RB16(extradata);
            extradata += 2;
            header_start[i] = extradata;
            extradata += header_len[i];
            if (overall_len > extradata_size - header_len[i])
                return -1;
            overall_len += header_len[i];
        }
    } else if (extradata_size >= 3 && extradata_size < INT_MAX - 0x1ff && extradata[0] == 2) {
        int overall_len = 3;
        extradata++;
        for (i=0; i<2; i++, extradata++) {
            header_len[i] = 0;
            for (; overall_len < extradata_size && *extradata==0xff; extradata++) {
                header_len[i] += 0xff;
                overall_len   += 0xff + 1;
            }
            header_len[i] += *extradata;
            overall_len   += *extradata;
            if (overall_len > extradata_size)
                return -1;
        }
        header_len[2] = extradata_size - overall_len;
        header_start[0] = extradata;
        header_start[1] = header_start[0] + header_len[0];
        header_start[2] = header_start[1] + header_len[1];
    } else {
        return -1;
    }
    return 0;
}

static char *xiph_extradata2config(AVCodecContext *c)
{
    char *config, *encoded_config;
    uint8_t *header_start[3];
    int headers_len, header_len[3], config_len;
    int first_header_size;

    switch (c->codec_id) {
    case AV_CODEC_ID_THEORA:
        first_header_size = 42;
        break;
    case AV_CODEC_ID_VORBIS:
        first_header_size = 30;
        break;
    default:
        qWarning() << "Unsupported Xiph codec ID";
        return NULL;
    }

    if (avpriv_split_xiph_headers(c->extradata, c->extradata_size,
        first_header_size, header_start,
        header_len) < 0) {
            av_log(c, AV_LOG_ERROR, "Extradata corrupt.\n");
            return NULL;
    }

    headers_len = header_len[0] + header_len[2];
    config_len = 4 +          // count
        3 +          // ident
        2 +          // packet size
        1 +          // header count
        2 +          // header size
        headers_len; // and the rest

    config = (char*) av_malloc(config_len);
    if (!config)
        goto xiph_fail;

    encoded_config = (char*) av_malloc(AV_BASE64_SIZE(config_len));
    if (!encoded_config) {
        av_free(config);
        goto xiph_fail;
    }

    config[0] = config[1] = config[2] = 0;
    config[3] = 1;
    config[4] = char((RTP_XIPH_IDENT >> 16) & 0xff);
    config[5] = char((RTP_XIPH_IDENT >>  8) & 0xff);
    config[6] = char((RTP_XIPH_IDENT      ) & 0xff);
    config[7] = (headers_len >> 8) & 0xff;
    config[8] = headers_len & 0xff;
    config[9] = 2;
    config[10] = header_len[0];
    config[11] = 0; // size of comment header; nonexistent
    memcpy(config + 12, header_start[0], header_len[0]);
    memcpy(config + 12 + header_len[0], header_start[2], header_len[2]);

    av_base64_encode(encoded_config, AV_BASE64_SIZE(config_len), (const uint8_t*) config, config_len);
    av_free(config);

    return encoded_config;

xiph_fail:
    qWarning() << "Not enough memory for configuration string";
    return NULL;
}

static char *sdp_write_media_attributes(
    char *buff,
    int size,
    AVCodecContext *c,
    int payload_type,
    AVFormatContext *fmt,
    const std::map<QString, QString>& streamParams )
{
    char* config = NULL;

    switch (c->codec_id) {
        case AV_CODEC_ID_H264: {
            int mode = 1;
            if (fmt && fmt->oformat->priv_class &&
                av_opt_flag_is_set(fmt->priv_data, "rtpflags", "h264_mode0"))
                mode = 0;
            av_strlcatf(buff, size, "a=rtpmap:%d H264/90000\r\n"
                                    "a=fmtp:%d packetization-mode=%d",
                                     payload_type, payload_type, mode );

            if (c->extradata_size)
                config = extradata2psets(c);
            if( config )
            {
                av_strlcatf( buff, size, config );
            }
            else
            {
                auto profileLevelIdIter = streamParams.find( Qn::PROFILE_LEVEL_ID_PARAM_NAME );
                if( profileLevelIdIter != streamParams.end() )
                    av_strlcatf( buff, size, "; profile-level-id=%s", profileLevelIdIter->second.toLatin1().constData() );

                auto spropParameterSetsIter = streamParams.find( Qn::SPROP_PARAMETER_SETS_PARAM_NAME );
                if( spropParameterSetsIter != streamParams.end() )
                    av_strlcatf( buff, size, "; sprop-parameter-sets=%s", spropParameterSetsIter->second.toLatin1().constData() );
            }

            av_strlcatf( buff, size, "\r\n" );
            break;
        }
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
            /* a=framesize is required by 3GPP TS 26.234 (PSS). It
             * actually specifies the maximum video size, but we only know
             * the current size. This is required for playback on Android
             * stagefright and on Samsung bada. */
            if (!fmt || !fmt->oformat->priv_class ||
                !av_opt_flag_is_set(fmt->priv_data, "rtpflags", "rfc2190") ||
                c->codec_id == AV_CODEC_ID_H263P)
            av_strlcatf(buff, size, "a=rtpmap:%d H263-2000/90000\r\n"
                                    "a=framesize:%d %d-%d\r\n",
                                    payload_type,
                                    payload_type, c->width, c->height);
            break;
        case AV_CODEC_ID_MPEG4:
            if (c->extradata_size) {
                config = extradata2config(c);
            }
            av_strlcatf(buff, size, "a=rtpmap:%d MP4V-ES/90000\r\n"
                                    "a=fmtp:%d profile-level-id=1%s\r\n",
                                     payload_type,
                                     payload_type, config ? config : "");
            break;
        case AV_CODEC_ID_AAC:
            if (fmt && fmt->oformat && fmt->oformat->priv_class &&
                av_opt_flag_is_set(fmt->priv_data, "rtpflags", "latm")) {
                config = latm_context2config(c);
                if (!config)
                    return NULL;
                av_strlcatf(buff, size, "a=rtpmap:%d MP4A-LATM/%d/%d\r\n"
                                        "a=fmtp:%d profile-level-id=%d;cpresent=0;config=%s\r\n",
                                         payload_type, c->sample_rate, c->channels,
                                         payload_type, latm_context2profilelevel(c), config);
            } else {
                if (c->extradata_size) {
                    config = extradata2config(c);
                } else {
                    /* FIXME: maybe we can forge config information based on the
                     *        codec parameters...
                     */
                    qWarning() << "AAC with no global headers is currently not supported.";
                    return NULL;
                }
                if (config == NULL) {
                    return NULL;
                }
                av_strlcatf(buff, size, "a=rtpmap:%d MPEG4-GENERIC/%d/%d\r\n"
                                        "a=fmtp:%d profile-level-id=1;"
                                        "mode=AAC-hbr;sizelength=13;indexlength=3;"
                                        "indexdeltalength=3%s\r\n",
                                         payload_type, c->sample_rate, c->channels,
                                         payload_type, config);
            }
            break;
        case AV_CODEC_ID_PCM_S16BE:
            if (payload_type >= RTP_PT_PRIVATE)
                av_strlcatf(buff, size, "a=rtpmap:%d L16/%d/%d\r\n",
                                         payload_type,
                                         c->sample_rate, c->channels);
            break;
        case AV_CODEC_ID_PCM_MULAW:
            if (payload_type >= RTP_PT_PRIVATE)
                av_strlcatf(buff, size, "a=rtpmap:%d PCMU/%d/%d\r\n",
                                         payload_type,
                                         c->sample_rate, c->channels);
            break;
        case AV_CODEC_ID_PCM_ALAW:
            if (payload_type >= RTP_PT_PRIVATE)
                av_strlcatf(buff, size, "a=rtpmap:%d PCMA/%d/%d\r\n",
                                         payload_type,
                                         c->sample_rate, c->channels);
            break;
        case AV_CODEC_ID_AMR_NB:
            av_strlcatf(buff, size, "a=rtpmap:%d AMR/%d/%d\r\n"
                                    "a=fmtp:%d octet-align=1\r\n",
                                     payload_type, c->sample_rate, c->channels,
                                     payload_type);
            break;
        case AV_CODEC_ID_AMR_WB:
            av_strlcatf(buff, size, "a=rtpmap:%d AMR-WB/%d/%d\r\n"
                                    "a=fmtp:%d octet-align=1\r\n",
                                     payload_type, c->sample_rate, c->channels,
                                     payload_type);
            break;
        case AV_CODEC_ID_VORBIS:
            if (c->extradata_size)
                config = xiph_extradata2config(c);
            else
                qWarning() << "Vorbis configuration info missing\n";
            if (!config)
                return NULL;

            av_strlcatf(buff, size, "a=rtpmap:%d vorbis/%d/%d\r\n"
                                    "a=fmtp:%d configuration=%s\r\n",
                                    payload_type, c->sample_rate, c->channels,
                                    payload_type, config);
            break;
        case AV_CODEC_ID_THEORA: {
            const char *pix_fmt;
            if (c->extradata_size)
                config = xiph_extradata2config(c);
            else
                qWarning() << "Theora configuation info missing";
            if (!config)
                return NULL;

            switch (c->pix_fmt) {
            case AV_PIX_FMT_YUV420P:
                pix_fmt = "YCbCr-4:2:0";
                break;
            case AV_PIX_FMT_YUV422P:
                pix_fmt = "YCbCr-4:2:2";
                break;
            case AV_PIX_FMT_YUV444P:
                pix_fmt = "YCbCr-4:4:4";
                break;
            default:
                qWarning() << "Unsupported pixel format.";
                return NULL;
            }

            av_strlcatf(buff, size, "a=rtpmap:%d theora/90000\r\n"
                                    "a=fmtp:%d delivery-method=inline; "
                                    "width=%d; height=%d; sampling=%s; "
                                    "configuration=%s\r\n",
                                    payload_type, payload_type,
                                    c->width, c->height, pix_fmt, config);
            break;
        }
        case AV_CODEC_ID_VP8:
            av_strlcatf(buff, size, "a=rtpmap:%d VP8/90000\r\n",
                                     payload_type);
            break;
        case AV_CODEC_ID_ADPCM_G722:
            if (payload_type >= RTP_PT_PRIVATE)
                av_strlcatf(buff, size, "a=rtpmap:%d G722/%d/%d\r\n",
                                         payload_type,
                                         8000, c->channels);
            break;
        case AV_CODEC_ID_ADPCM_G726: {
            if (payload_type >= RTP_PT_PRIVATE)
                av_strlcatf(buff, size, "a=rtpmap:%d G726-%d/%d\r\n",
                                         payload_type,
                                         c->bits_per_coded_sample*8,
                                         c->sample_rate);
            break;
        }
        /*
        case AV_CODEC_ID_ILBC:
            av_strlcatf(buff, size, "a=rtpmap:%d iLBC/%d\r\n"
                                    "a=fmtp:%d mode=%d\r\n",
                                     payload_type, c->sample_rate,
                                     payload_type, c->block_align == 38 ? 20 : 30);
            break;
        */
        default:
            /* Nothing special to do here... */
            break;
    }

    av_free(config);

    return buff;
}


QnUniversalRtpEncoder::QnUniversalRtpEncoder(
    QnCommonModule* commonModule,
    QnConstAbstractMediaDataPtr media,
    AVCodecID transcodeToCodec,
    const QSize& videoSize,
    const QnLegacyTranscodingSettings& extraTranscodeParams)
:
    m_outputBuffer(CL_MEDIA_ALIGNMENT, 0),
    m_outputPos(0),
    packetIndex(0),
    //m_firstTime(0),
    //m_isFirstPacket(true),
    m_isOpened(false)
{
    if( m_transcoder.setContainer("rtp") != 0 )
        return; //m_isOpened = false
    m_transcoder.setPacketizedMode(true);
    m_codec = transcodeToCodec != AV_CODEC_ID_NONE ? transcodeToCodec : media->compressionType;
    QnTranscoder::TranscodeMethod method;
    m_isVideo = media->dataType == QnAbstractMediaData::VIDEO;
    if (m_isVideo)
        method = transcodeToCodec != AV_CODEC_ID_NONE ? QnTranscoder::TM_FfmpegTranscode : QnTranscoder::TM_DirectStreamCopy;
    else
        method = media->compressionType == transcodeToCodec ? QnTranscoder::TM_DirectStreamCopy : QnTranscoder::TM_FfmpegTranscode;

    if (media->dataType == QnAbstractMediaData::VIDEO) {
        m_transcoder.setTranscodingSettings(extraTranscodeParams);
        m_transcoder.setVideoCodec(m_codec, method, Qn::QualityNormal, videoSize);
    }
    else {
        m_transcoder.setAudioCodec(m_codec, method);
    }

    if (commonModule->isTranscodeDisabled() && method != QnTranscoder::TM_DirectStreamCopy)
    {
        m_isOpened = false;
        qWarning() << "Video transcoding is disabled in the server settings. Feature unavailable.";
    }
    else if (m_isVideo)
    {
        m_isOpened = m_transcoder.open(std::dynamic_pointer_cast<const QnCompressedVideoData>(media), QnConstCompressedAudioDataPtr()) == 0;
    }
    else
    {
        m_isOpened = m_transcoder.open(QnConstCompressedVideoDataPtr(), std::dynamic_pointer_cast<const QnCompressedAudioData>(media)) == 0;
    }
}

QByteArray QnUniversalRtpEncoder::getAdditionSDP( const std::map<QString, QString>& streamParams )
{
    char buffer[1024*16];
    memset(buffer, 0, sizeof(buffer));
    AVCodecContext* ctx = m_isVideo ? m_transcoder.getVideoCodecContext() : m_transcoder.getAudioCodecContext();
    sdp_write_media_attributes(
        buffer,
        sizeof(buffer),
        ctx,
        getPayloadtype(),
        m_transcoder.getFormatContext(),
        streamParams );
    return QByteArray(buffer);
}

void QnUniversalRtpEncoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    m_outputBuffer.clear();
    m_outputPos = 0;
    packetIndex = 0;
    m_transcoder.transcodePacket(media, &m_outputBuffer);
}

bool QnUniversalRtpEncoder::getNextPacket(QnByteArray& sendBuffer)
{
    const QVector<int> packets = m_transcoder.getPacketsSize();
    if (m_outputPos >= (int) m_outputBuffer.size() - RtpHeader::RTP_HEADER_SIZE || packetIndex >= packets.size())
        return false;

    /*
    if (packets[packetIndex] >= 12) {
        quint32* srcBuffer = (quint32*) (m_outputBuffer.data() + m_outputPos);
        RtpHeader* rtpHeader = (RtpHeader*) srcBuffer;
        bool isRtcp = rtpHeader->payloadType >= 72 && rtpHeader->payloadType <= 76;
        if (!isRtcp)
            rtpHeader->ssrc = htonl(getSSRC());
        else
            srcBuffer[1] = htonl(getSSRC());
    }
    */

    sendBuffer.write(m_outputBuffer.data() + m_outputPos, packets[packetIndex]);
    m_outputPos += packets[packetIndex];
    packetIndex++;
    return true;
}

void QnUniversalRtpEncoder::init()
{
}

quint32 QnUniversalRtpEncoder::getSSRC()
{
    return 15000 + (m_isVideo ? 0 : 1);
}

bool QnUniversalRtpEncoder::getRtpMarker()
{
    // unused
    return false;
}

quint32 QnUniversalRtpEncoder::getFrequency()
{
    for (int i = 0; AVRtpPayloadTypes[i].pt >= 0; ++i)
    {
        if (AVRtpPayloadTypes[i].codec_id == m_codec && AVRtpPayloadTypes[i].clock_rate != -1)
            return AVRtpPayloadTypes[i].clock_rate;
    }
    if (m_transcoder.getFormatContext())
    {
        //AVCodecContext* codecCtx =  m_transcoder.getFormatContext()->streams[0]->codec;
        //if (codecCtx)
        //    return codecCtx->time_base.den / codecCtx->time_base.num;
        AVStream* stream = m_transcoder.getFormatContext()->streams[0];
        return stream->time_base.den / stream->time_base.num;
    }

    return 90000;
}

quint8 QnUniversalRtpEncoder::getPayloadtype()
{
    // static payload type
    for (int i = 0; AVRtpPayloadTypes[i].pt >= 0; ++i)
    {
        if (AVRtpPayloadTypes[i].codec_id == m_codec)
            return AVRtpPayloadTypes[i].pt;
    }
    // dynamic payload type
    return RTP_PT_PRIVATE + (m_isVideo ? 0 : 1);
}

QString QnUniversalRtpEncoder::getName()
{
    for (int i = 0; AVRtpPayloadTypes[i].pt >= 0; ++i)
    {
        if (AVRtpPayloadTypes[i].codec_id == m_codec)
            return AVRtpPayloadTypes[i].enc_name;
    }
    return QString();
}

bool QnUniversalRtpEncoder::isOpened() const
{
    return m_isOpened;
}

void QnUniversalRtpEncoder::setUseRealTimeOptimization(bool value)
{
    m_transcoder.setUseRealTimeOptimization(value);
}
