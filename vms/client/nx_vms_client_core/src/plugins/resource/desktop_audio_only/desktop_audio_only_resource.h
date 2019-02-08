#pragma once
#include <plugins/resource/desktop_camera/desktop_resource_base.h>

class QnDesktopAudioOnlyResource : public QnDesktopResource
{
    Q_OBJECT
public:
    QnDesktopAudioOnlyResource();
    virtual ~QnDesktopAudioOnlyResource();

    virtual bool isRendererSlow() const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
            const QnAbstractStreamDataProvider *dataProvider) const override;

    bool hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);
};
