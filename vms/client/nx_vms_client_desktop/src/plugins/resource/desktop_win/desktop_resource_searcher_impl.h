#pragma once

#include <plugins/resource/desktop_camera/abstract_desktop_resource_searcher_impl.h>

struct IDirect3D9;
class QGLWidget;

class QnDesktopResourceSearcherImpl: public QnAbstractDesktopResourceSearcherImpl
{
public:
    QnDesktopResourceSearcherImpl(QGLWidget* mainWidget);
    virtual ~QnDesktopResourceSearcherImpl() override;

    virtual QnResourceList findResources() override;

private:
    IDirect3D9* m_pD3D = nullptr;
    QGLWidget* m_mainWidget = nullptr;
};
