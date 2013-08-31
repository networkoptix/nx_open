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

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool isInitialized() const;

    virtual bool shoudResolveConflicts() const override;

    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) override;

};

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
