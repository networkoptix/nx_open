// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../desktop_resource.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API DesktopAudioOnlyResource: public DesktopResource
{
    Q_OBJECT

public:
    virtual bool isRendererSlow() const override;
    virtual AudioLayoutConstPtr getAudioLayout(
        const QnAbstractStreamDataProvider* dataProvider) const override;

    bool hasVideo(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    virtual QnAbstractStreamDataProvider* createDataProvider(Qn::ConnectionRole role) override;
};

} // namespace nx::vms::client::core
