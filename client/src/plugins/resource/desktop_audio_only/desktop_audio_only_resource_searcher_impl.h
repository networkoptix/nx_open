#pragma once

#if !defined(Q_OS_WIN)
#include <core/resource/resource_fwd.h>

class QGLWidget;

class QnDesktopResourceSearcherImpl
{

public:
    QnDesktopResourceSearcherImpl(QGLWidget* mainWindow = 0);
    ~QnDesktopResourceSearcherImpl();

    QnResourceList findResources();
};
#endif

