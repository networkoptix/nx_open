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

public:
    static const char* MANUFACTURE;

    static const int MAX_STREAMS = 2;

    QnDesktopCameraResource();
    ~QnDesktopCameraResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems) {}

    virtual CameraDiagnostics::Result initInternal() override;

    virtual bool shoudResolveConflicts() const override { return false; }

    virtual bool setRelayOutputState(const QString& ouputID, bool activate, unsigned int autoResetTimeoutMS = 0) override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping) override {}

    virtual bool isResourceAccessible() override;

    QString gePhysicalIdPrefix() const;
    QString getUserName() const;
    const QnResourceAudioLayout* getAudioLayout(const QnAbstractStreamDataProvider* dataProvider);
};
typedef QSharedPointer<QnDesktopCameraResource> QnDesktopCameraResourcePtr;

#endif //ENABLE_DESKTOP_CAMERA

#endif // _DESKTOP_CAMERA_RESOURCE_SEARCHER_H__
