#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119

#ifdef ENABLE_ARECONT

#include "av_client_pull.h"
#include "../tools/simple_tftp_client.h"

class QnAbstractMediaDataPacket;

//single sensor TFTP reader
// arecont vision TFTP stack is faster (for JPEG image; for H.264 they are almost equal )( you can get more fps with it )
// so if it's possible to use TFP ( local networks ) it's btter be TFTP but HTTP
class AVClientPullSSTFTPStreamreader : public QnPlAVClinetPullStreamReader
{
public:
    explicit AVClientPullSSTFTPStreamreader(const QnResourcePtr& res);

    ~AVClientPullSSTFTPStreamreader();

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    /*!
        Reopens media stream, if it not opened yet.
        Blocks for media stream open attempt
        \return error code and filled \a errorParams with parameters
        \note If stream is opened (\a CLServerPushStreamReader::isStreamOpened() returns true) \a CameraDiagnostics::ErrorCode::noError is returned immediately
    */
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;

protected:

    virtual QnAbstractMediaDataPtr getNextData();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

protected:

    int m_last_width;
    int m_last_height;
    int m_last_cam_width;
    int m_last_cam_height;
    bool m_last_resolution;
    unsigned int m_timeout;
    bool m_black_white; // for dual sensor only 

    bool m_panoramic;
    bool m_dualsensor;
    QString m_model;
    CLSimpleTFTPClient* m_tftp_client;
    CameraDiagnostics::Result m_prevDataReadResult;
};

#endif // ENABLE_ARECONT
#endif //cpull_httpreader_1119
