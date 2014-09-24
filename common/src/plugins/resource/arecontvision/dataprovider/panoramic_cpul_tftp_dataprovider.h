#ifndef panoramic_cpull_httpreader_1800
#define panoramic_cpull_httpreader_1800

#ifdef ENABLE_ARECONT

#include "av_client_pull.h"

class CLSimpleTFTPClient;

//single sensor TFTP reader
// arecont vision TFTP stack is faster (for JPEG image; for H.264 they are almost equal )( you can get more fps with it )
// so if it's possible to use TFP ( local networks ) it's btter be TFTP but HTTP
class AVPanoramicClientPullSSTFTPStreamreader : public QnPlAVClinetPullStreamReader
{
public:
    explicit AVPanoramicClientPullSSTFTPStreamreader(const QnResourcePtr& res);

    ~AVPanoramicClientPullSSTFTPStreamreader();

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;

    virtual void updateStreamParamsBasedOnFps() override;
    virtual void updateStreamParamsBasedOnQuality() override; 
    virtual void beforeRun() override;
protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    virtual bool needKeyData() const;

protected:

    int m_last_width; // to avoid frequent moving data!!
    int m_last_height;
    bool m_last_resolution;

    unsigned int m_timeout;
    
    bool m_panoramic;
    bool m_dualsensor;
    QString m_model;

    CLSimpleTFTPClient* m_tftp_client;

    int m_motionData;
};

#endif

#endif //cpull_httpreader_1119
