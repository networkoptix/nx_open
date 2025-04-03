// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/screen_recording/abstract_desktop_resource_searcher_impl.h>

struct IDirect3D9;
class QOpenGLWidget;

namespace nx::vms::client::desktop {

class WindowsDesktopResourceSearcherImpl: public QnAbstractDesktopResourceSearcherImpl
{
public:
    WindowsDesktopResourceSearcherImpl();
    virtual ~WindowsDesktopResourceSearcherImpl() override;

    virtual QnResourceList findResources() override;

private:
    IDirect3D9* const m_pD3D;
};

} // namespace nx::vms::client::desktop
