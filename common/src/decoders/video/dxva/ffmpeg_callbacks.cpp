#include "ffmpeg_callbacks.h"

#include "base/log.h"
#include "dxva.h"

int FFMpegCallbacks::ffmpeg_GetFrameBuf(struct AVCodecContext *p_context,AVFrame *p_ff_pic)
{
    DecoderContext* dec = (DecoderContext*)p_context->opaque;

    DxvaSupportObject *va = dec->va();

    //    p_context->opaque = 0;

    //////////////////////////////////////////////////////////////////////////////////////

    if (va)
    {
        FOURCC chroma;
        if (!dec->setup(&p_context->hwaccel_context, &chroma, p_context->width, p_context->height ))
        {
            cl_log.log(cl_logERROR, "vlc_va_Setup failed" );
            return -1;
        }

        p_ff_pic->type = FF_BUFFER_TYPE_USER;
        /* FIXME what is that, should give good value */
        p_ff_pic->age = 256*256*256*64; // FIXME FIXME from ffmpeg

        if(!va->get(p_ff_pic))
        {
            cl_log.log(cl_logERROR, "VaGrabSurface failed");
            return -1;
        }

        return 0;
    }

    return avcodec_default_get_buffer( p_context, p_ff_pic );
}

void FFMpegCallbacks::ffmpeg_ReleaseFrameBuf(struct AVCodecContext *p_context, AVFrame *p_ff_pic)
{
    DecoderContext* dec = (DecoderContext*)p_context->opaque;

    DxvaSupportObject *va = dec->va();

    if (va)
    {
        va->release(p_ff_pic);

        for( int i = 0; i < 4; i++ )
            p_ff_pic->data[i] = NULL;

        return;
    }

    avcodec_default_release_buffer(p_context, p_ff_pic);
}

PixelFormat FFMpegCallbacks::ffmpeg_GetFormat( AVCodecContext *p_codec, const enum PixelFormat *pi_fmt )
{
    DecoderContext* dec = (DecoderContext*)p_codec->opaque;

    if (dec->va())
    {
        dec->close();
    }

    if (!dec->isHardwareAcceleration())
        return avcodec_default_get_format (p_codec, pi_fmt);

    DxvaSupportObject *va = dec->va();

    const char *ppsz_name[PIX_FMT_NB];

    ppsz_name[PIX_FMT_DXVA2_VLD] = "PIX_FMT_DXVA2_VLD",
    ppsz_name[PIX_FMT_YUYV422] = "PIX_FMT_YUYV422";
    ppsz_name[PIX_FMT_YUV420P] = "PIX_FMT_YUV420P";

    /* Try too look for a supported hw acceleration */
    for( int i = 0; pi_fmt[i] != PIX_FMT_NONE; i++ )
    {
        if( pi_fmt[i] == PIX_FMT_DXVA2_VLD && p_codec->width > 0 && p_codec->height > 0)
        {
            cl_log.log(cl_logDEBUG1, "Available decoder output format %d (%s)", pi_fmt[i], ppsz_name[pi_fmt[i]] ? ppsz_name[pi_fmt[i]] : "Unknown" );

            if (p_codec->width > 1920 || p_codec->height > 1088)
            {
                cl_log.log(cl_logDEBUG1, "Resolution is too high for dxva");
                break;
            }

            cl_log.log(cl_logDEBUG1, "Trying DXVA2");
            if (!dec->newDxva(p_codec->codec_id))
            {
                cl_log.log(cl_logWARNING, "Failed to open DXVA2" );
                break;
            }

            va = dec->va();

            /* We try to call Setup when possible to detect errors when
            * possible (later is too late) */
            FOURCC chroma;
            if( va && !dec->setup( 
                &p_codec->hwaccel_context,
                &chroma, // &p_dec->fmt_out.video.i_chroma
                p_codec->width, p_codec->height ) )
            {
                cl_log.log(cl_logERROR, "Setup failed" );
                dec->close();
                va = NULL;
            }

            if (va)
            {
                cl_log.log(cl_logINFO, "Using %s for hardware decoding.", va->description().toLatin1().constData() );

                return pi_fmt[i];
            }
        }
    }

    /* Fallback to default behaviour */
    return avcodec_default_get_format (p_codec, pi_fmt);
}
