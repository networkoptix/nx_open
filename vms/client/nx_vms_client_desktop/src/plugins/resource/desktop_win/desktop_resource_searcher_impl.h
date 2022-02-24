// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <plugins/resource/desktop_camera/abstract_desktop_resource_searcher_impl.h>

struct IDirect3D9;
class QOpenGLWidget;

class QnDesktopResourceSearcherImpl: public QnAbstractDesktopResourceSearcherImpl
{
public:
    QnDesktopResourceSearcherImpl(QOpenGLWidget* mainWidget);
    virtual ~QnDesktopResourceSearcherImpl() override;

    virtual QnResourceList findResources() override;

private:
    QOpenGLWidget* const m_mainWidget;
    IDirect3D9* const m_pD3D;
};
