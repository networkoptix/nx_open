// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <plugins/resource/desktop_camera/desktop_resource_base.h>

class NX_VMS_CLIENT_CORE_API QnDesktopAudioOnlyResource : public QnDesktopResource
{
    Q_OBJECT
public:
    QnDesktopAudioOnlyResource();
    virtual ~QnDesktopAudioOnlyResource();

    virtual bool isRendererSlow() const override;
    virtual AudioLayoutConstPtr getAudioLayout(
            const QnAbstractStreamDataProvider* dataProvider) const override;

    bool hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);
};
