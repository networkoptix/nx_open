#ifndef cl_av_clien_pull1529
#define cl_av_clien_pull1529

#ifdef ENABLE_ARECONT

#include <providers/cpull_media_stream_provider.h>
#include "basic_av_stream_reader.h"


struct AVLastPacketSize
{
    int x0, y0, width, height;

    AVLastPacketSize()
    :   x0(0), y0(0), width(0), height(0)
    {
    }
};

class QnPlAVClinetPullStreamReader
:
    public QnBasicAvStreamReader<QnClientPullMediaStreamProvider>
{
    typedef QnBasicAvStreamReader<QnClientPullMediaStreamProvider> parent_type;

public:
    QnPlAVClinetPullStreamReader(const QnResourcePtr& res);
    virtual ~QnPlAVClinetPullStreamReader();

protected:
    int getBitrateMbps() const;
    bool isH264() const;
    bool isCameraControlRequired() const override;

protected:
    // in av cameras you do not know the size of the frame in advance;
    //so we can save a lot of memory by receiving all frames in this buff
    // but will slow down a bit coz of extra memcpy ( I think not much )
    QnByteArray m_videoFrameBuff;
    bool m_panoramic;
    bool m_dualsensor;
};

#endif

#endif //cl_av_clien_pull1529

