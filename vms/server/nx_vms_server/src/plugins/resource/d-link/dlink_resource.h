#pragma once

#ifdef ENABLE_DLINK

#include <nx/network/deprecated/simple_http_client.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/vms/server/resource/camera.h>

struct QnDlink_ProfileInfo
{
    QnDlink_ProfileInfo(): number(0) {}

    int number;
    QString url;
    QByteArray codec;
};

struct QnDlink_cam_info
{
    QnDlink_cam_info();

    void clear();

    // returns resolution with width not less than width
    QSize resolutionCloseTo(int width) const;

    // returns next up bitrate
    QByteArray bitrateCloseTo(int val);

    // returns next up frame rate
    int frameRateCloseTo(int fr);

    QSize primaryStreamResolution() const;
    QSize secondaryStreamResolution() const;

    //QString hasH264;// some cams have H.264, some H264
    //bool hasMPEG4;
    QList<QSize> resolutions;

    QMap<int, QByteArray> possibleBitrates;
    QList<int> possibleFps;
    QList<QByteArray> possibleQualities;
    QVector<QnDlink_ProfileInfo> profiles;
};

class QnPlDlinkResource: public nx::vms::server::resource::Camera
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnPlDlinkResource(QnMediaServerModule* serverModule);

    //!Implementation of QnNetworkResource::checkIfOnlineAsync
    virtual void checkIfOnlineAsync( std::function<void(bool)> completionHandler ) override;

    virtual QString getDriverName() const override;

    QnDlink_cam_info getCamInfo() const;


    virtual void setMotionMaskPhysical(int channel) override;

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDriver(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override; // does a lot of physical work
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCroppingPhysical(QRect cropping);


protected:
    QnDlink_cam_info  m_camInfo;
};

#endif // ENABLE_DLINK
