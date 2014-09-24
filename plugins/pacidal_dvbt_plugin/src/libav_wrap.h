#ifndef LIBAV_WRAP_H
#define LIBAV_WRAP_H

#include <memory>

extern "C"
{
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#if 0
static int interruptFindInputFormat(void* opaque)
{
    static const unsigned MAX_INTERRUPTS = 1000;

    unsigned * const pCounter = reinterpret_cast<unsigned * const>(opaque);
    if( *pCounter >= MAX_INTERRUPTS)
        return 1;

    ++(*pCounter);
    return 0;
}
#endif
}

namespace pacidal
{
    constexpr static const char * EXPECTED_IFORMAT = "mpegts";
    static AVInputFormat * sExpectedFormat = av_find_input_format( EXPECTED_IFORMAT );

    //!
    class LibAV
    {
    public:
        typedef int (*FuncT)(void* opaque, uint8_t* buf, int bufSize);

        LibAV(void * opaque, FuncT readFunc, unsigned frameSize = 32*1024)
        :
            m_avIO(nullptr),
            m_avContext(avformat_alloc_context())
            //m_interruptCounter(0)
        {
            m_avIO = avio_alloc_context(
                (uint8_t*)av_malloc(frameSize), frameSize, 0, opaque, readFunc, nullptr, nullptr);

            m_avIO->seekable = 0;
            m_avContext->pb = m_avIO;
            //m_avContext->interrupt_callback.callback = interruptFindInputFormat;
            //m_avContext->interrupt_callback.opaque = &m_interruptCounter;
            m_avContext->iformat = sExpectedFormat;

            readFunc( opaque, m_avIO->buffer, frameSize );

            if( avformat_open_input(&m_avContext, "", nullptr, nullptr) < 0 ) // free inside
                return;

            avformat_find_stream_info(m_avContext, nullptr);
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

        float fps(unsigned streamNum) const
        {
            AVStream * s = stream( streamNum );
            if( s )
                return float(s->avg_frame_rate.num) / s->avg_frame_rate.den;
            return 0;
        }

        void resolution(unsigned streamNum, int& width, int& height) const
        {
            AVStream * s = stream( streamNum );
            if( s && s->codec )
            {
                width = s->codec->width;
                height = s->codec->height;
            }
        }

        double secTimeBase(unsigned streamNum) const
        {
            AVStream * s = stream( streamNum );
            if( s )
                return av_q2d( s->time_base );

            static AVRational def = { 1, 90000 };
            return av_q2d( def );
        }

        static void freePacket(AVPacket * pkt)
        {
            if (pkt)
                av_free_packet(pkt);
        }

        bool isOK() const { return m_avContext && m_avIO; }
        bool discardCorrupt() const { return m_avContext->flags & AVFMT_FLAG_DISCARD_CORRUPT; }

        std::mutex& mutex() { return m_mutex; }

    private:
        AVIOContext* m_avIO;
        AVFormatContext* m_avContext;
        std::mutex m_mutex;
        //unsigned m_interruptCounter;

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

        AVStream * stream(unsigned num) const
        {
            if (m_avContext && num < streamsCount())
                return m_avContext->streams[num];
            return nullptr;
        }
    };
}

#endif //LIBAV_WRAP
