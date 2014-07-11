#ifndef ___DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
#define ___DESKTOP_CAMERA_RESOURCE_SEARCHER_H__

#ifdef ENABLE_DESKTOP_CAMERA

#include <QMap>
#include <QMutex>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "core/datapacket/media_data_packet.h"

class QnDesktopCameraResource
:
    public QnPhysicalCameraResource
{
    Q_OBJECT

    typedef QnPhysicalCameraResource base_type;

public:
    static const QString MANUFACTURE;

    static const int MAX_STREAMS = 2;

    QnDesktopCameraResource();
    QnDesktopCameraResource(const QString &userName);
    ~QnDesktopCameraResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems) { Q_UNUSED(frames) Q_UNUSED(timems) }

    virtual bool setRelayOutputState(const QString& outputID, bool activate, unsigned int autoResetTimeoutMS = 0) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual bool isResourceAccessible() override;

    QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider);

    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;
};

#endif //ENABLE_DESKTOP_CAMERA

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
