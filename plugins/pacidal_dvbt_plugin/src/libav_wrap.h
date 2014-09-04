#ifndef LIBAV_WRAP_H
#define LIBAV_WRAP_H

#include <memory>


#if 0 // dummy

#define AV_PKT_FLAG_KEY 0x1
#define AV_PKT_FLAG_CORRUPT 0x2

struct AVPacket
{
    int64_t pts;
    int64_t dts;
    uint8_t *data;
    int   size;
    int   stream_index;
    int   flags;
};

struct AVStream
{
    struct
    {
        unsigned num;
        unsigned den;
    } avg_frame_rate;
    struct
    {
        unsigned width;
        unsigned height;
    } * codec;
};

namespace pacidal
{
    //!
    class LibAV
    {
    public:
        typedef int (*FuncT)(void* opaque, uint8_t* buf, int bufSize);

        LibAV(void * opaque, FuncT func, unsigned frameSize)
        {
        }

        ~LibAV()
        {
        }

        std::shared_ptr<AVPacket> nextFrame()
        {
            return std::make_shared<AVPacket>();
        }

        unsigned streamsCount() const
        {
            return 0;
        }

        AVStream * stream(unsigned num) const
        {
            return nullptr;
        }

        static void freePacket(AVPacket * pkt)
        {
        }
    };
}

#else //

extern "C"
{
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace pacidal
{
    //!
    class LibAV
    {
    public:
        typedef int (*FuncT)(void* opaque, uint8_t* buf, int bufSize);

        LibAV(void * opaque, FuncT func, unsigned frameSize = 32*1024)
        :
            m_avIO(nullptr),
            m_avContext(avformat_alloc_context())
        {
            m_avIO = avio_alloc_context(
                (uint8_t*)av_malloc(frameSize), frameSize, 0, opaque, func, nullptr, nullptr);

            m_avIO->seekable = 0;
            m_avContext->pb = m_avIO;
            m_avContext->iformat = av_find_input_format("mpegts");

            func(opaque, m_avIO->buffer, frameSize);

            if (avformat_open_input(&m_avContext, "x.ts", nullptr, nullptr) < 0 ||
                avformat_find_stream_info(m_avContext, nullptr) < 0)
            {
                free();
            }
        }

        ~LibAV()
        {
            free();
        }

        int nextFrame(AVPacket* packet)
        {
            return av_read_frame(m_avContext, packet);
        }

        unsigned streamsCount() const
        {
            if (m_avContext)
                return m_avContext->nb_streams;
            return 0;
        }

        AVStream * stream(unsigned num) const
        {
            if (m_avContext && num < streamsCount())
                return m_avContext->streams[num];
            return nullptr;
        }

        void freePacket(AVPacket * pkt)
        {
            if (pkt)
            {
                if (m_avContext->flags & AVFMT_FLAG_DISCARD_CORRUPT && pkt->flags & AV_PKT_FLAG_CORRUPT)
                {
                    // do nothing
                }
                else
                    av_free_packet(pkt);
            }
        }

    private:
        AVIOContext* m_avIO;
        AVFormatContext* m_avContext;

        void free()
        {
            if (m_avIO)
            {
                av_free(m_avIO->buffer);
                av_free(m_avIO);
            }

            if (m_avContext)
                avformat_free_context(m_avContext);

            m_avIO = nullptr;
            m_avContext = nullptr;
        }
    };
}
#endif

#endif //LIBAV_WRAP
