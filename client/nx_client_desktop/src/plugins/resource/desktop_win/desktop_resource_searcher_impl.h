#pragma once

#include <core/resource/resource_fwd.h>

struct IDirect3D9;
class QGLWidget;

class QnDesktopResourceSearcherImpl
{
public:
    QnDesktopResourceSearcherImpl(QGLWidget* mainWidget);
    ~QnDesktopResourceSearcherImpl();
    QnResourceList findResources();

private:
    IDirect3D9 *m_pD3D;
    QGLWidget* m_mainWidget;
};
