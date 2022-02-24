// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <plugins/resource/desktop_camera/abstract_desktop_resource_searcher_impl.h>

struct QnDesktopAudioOnlyResourceSearcherImpl: public QnAbstractDesktopResourceSearcherImpl
{
    virtual QnResourceList findResources() override;
};
