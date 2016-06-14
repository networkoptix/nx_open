#ifndef ___DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
#define ___DESKTOP_CAMERA_RESOURCE_SEARCHER_H__

#ifdef ENABLE_DESKTOP_CAMERA

#include <QMap>
#include <utils/thread/mutex.h>

#include "core/resource/camera_resource.h"

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

    virtual void setIframeDistance(int frames, int timems) override { Q_UNUSED(frames) Q_UNUSED(timems) }

    virtual bool setRelayOutputState(const QString& outputID, bool activate, unsigned int autoResetTimeoutMS = 0) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const override;

    virtual bool mergeResourcesIfNeeded(const QnNetworkResourcePtr &source) override;

    virtual bool isReadyToDetach() const override;

    virtual bool isInitialized() const override { return true; }
};

#endif //ENABLE_DESKTOP_CAMERA

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
