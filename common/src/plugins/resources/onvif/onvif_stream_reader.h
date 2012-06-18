#ifndef onvif_stream_reader_h
#define onvif_stream_reader_h

#include "onvif_helper.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"

class _onvifMedia__GetProfilesResponse;
class _onvifMedia__GetProfileResponse;
class onvifXsd__Profile;
class onvifXsd__VideoEncoderConfiguration;

class QnOnvifStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
    class VideoEncoderChange
    {
    public:
        VideoEncoderChange() {};
        virtual ~VideoEncoderChange() {};

        virtual void doChange(onvifXsd__VideoEncoderConfiguration* config, const QnOnvifStreamReader& reader, bool isPrimary) = 0;
        virtual void undoChange(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary) = 0;
        virtual QString description() = 0;
    };

    class VideoEncoderNameChange: public VideoEncoderChange
    {
        static const char* DESCRIPTION;

        QString oldName;
    public:
        VideoEncoderNameChange() {};
        virtual ~VideoEncoderNameChange() {};

        void doChange(onvifXsd__VideoEncoderConfiguration* config, const QnOnvifStreamReader& reader, bool isPrimary);
        void undoChange(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary);
        QString description();
    };

    class VideoEncoderFpsChange: public VideoEncoderChange
    {
        static const char* DESCRIPTION;

        int oldFps;
    public:
        VideoEncoderFpsChange() {};
        virtual ~VideoEncoderFpsChange() {};

        void doChange(onvifXsd__VideoEncoderConfiguration* config, const QnOnvifStreamReader& reader, bool isPrimary);
        void undoChange(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary);
        QString description();
    };

    class VideoEncoderQualityChange: public VideoEncoderChange
    {
        static const char* DESCRIPTION;

        float oldQuality;
    public:
        VideoEncoderQualityChange() {};
        virtual ~VideoEncoderQualityChange() {};

        void doChange(onvifXsd__VideoEncoderConfiguration* config, const QnOnvifStreamReader& reader, bool isPrimary);
        void undoChange(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary);
        QString description();
    };

    class VideoEncoderResolutionChange: public VideoEncoderChange
    {
        static const char* DESCRIPTION;

        ResolutionPair oldResolution;
    public:
        VideoEncoderResolutionChange() {};
        virtual ~VideoEncoderResolutionChange() {};

        void doChange(onvifXsd__VideoEncoderConfiguration* config, const QnOnvifStreamReader& reader, bool isPrimary);
        void undoChange(onvifXsd__VideoEncoderConfiguration* config, bool isPrimary);
        QString description();
    };

    typedef QList<VideoEncoderChange*> VideoEncoderChanges;

public:
    QnOnvifStreamReader(QnResourcePtr res);
    virtual ~QnOnvifStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;
private:

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    void processTriggerData(const quint8* payload, int len);

    void fillMotionInfo(const QRect& rect);
    bool isGotFrame(QnCompressedVideoDataPtr videoData);

    const QString updateCameraAndfetchStreamUrl() const;
    const QString updateCameraAndfetchStreamUrl(bool isPrimary) const;

    //Returned ptr can be used only when response is not destroyed!
    onvifXsd__Profile* findAppropriateProfile(const _onvifMedia__GetProfilesResponse& response, bool isPrimary) const;

    void fillVideoEncoderChanges(VideoEncoderChanges& changes) const;
    void clearVideoEncoderChanges(VideoEncoderChanges& changes) const;
    const QString normalizeStreamSrcUrl(const std::string& src) const;
    void printProfile(const _onvifMedia__GetProfileResponse& response, bool isPrimary) const;
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_multiCodec;
    QnPlOnvifResourcePtr m_onvifRes;
};

#endif // onvif_stream_reader_h
