// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../desktop_resource.h"

namespace nx::vms::client::core {

class DesktopAudioOnlyResource: public DesktopResource
{
    Q_OBJECT

public:
    DesktopAudioOnlyResource();
    virtual ~DesktopAudioOnlyResource();

    virtual bool isRendererSlow() const override;
    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

    bool hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    static QnAbstractStreamDataProvider* createDataProvider(
        const QnResourcePtr& resource,
        Qn::ConnectionRole role);
};

} // namespace nx::vms::client::core
