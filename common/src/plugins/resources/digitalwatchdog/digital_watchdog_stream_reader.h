#ifndef wd_stream_reader_h_22161
#define wd_stream_reader_h_22161

#include "../onvif/dataprovider/rtp_stream_provider.h"

class QnPlDWDStreamReader : public QnRtpStreamReader
{
public:
    QnPlDWDStreamReader(QnResourcePtr res);
    virtual ~QnPlDWDStreamReader();

    void updateStreamParamsBasedOnQuality() override;
    void updateStreamParamsBasedOnFps() override;

protected:
    virtual void openStream() override;
private:
};

#endif //wd_stream_reader_h_22161