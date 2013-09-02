#ifndef ___DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
#define ___DESKTOP_CAMERA_RESOURCE_SEARCHER_H__

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

public:
    static const char* MANUFACTURE;

    static const int MAX_STREAMS = 2;

    QnDesktopCameraResource();
    ~QnDesktopCameraResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems) {}

    bool isInitialized() const;

    virtual bool shoudResolveConflicts() const override { return false; }

    virtual bool setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS = 0) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping) override {}

    virtual bool isResourceAccessible() override;
};
typedef QSharedPointer<QnDesktopCameraResource> QnDesktopCameraResourcePtr;

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
