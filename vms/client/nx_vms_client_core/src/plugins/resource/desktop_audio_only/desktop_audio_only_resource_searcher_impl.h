#pragma once

#include <plugins/resource/desktop_camera/abstract_desktop_resource_searcher_impl.h>

struct QnDesktopAudioOnlyResourceSearcherImpl: public QnAbstractDesktopResourceSearcherImpl
{
    virtual QnResourceList findResources() override;
};
