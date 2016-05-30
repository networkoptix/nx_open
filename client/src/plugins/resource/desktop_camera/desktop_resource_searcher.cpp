#include "desktop_resource_searcher.h"
#ifdef Q_OS_WIN
    #include <plugins/resource/desktop_win/desktop_resource_searcher_impl.h>
#else
    #include <plugins/resource/desktop_audio_only/desktop_audio_only_resource_searcher_impl.h>
#endif

namespace {
    const QString kManufacture = lit("Network Optix");
}

QnDesktopResourceSearcher::QnDesktopResourceSearcher(QGLWidget* mainWidget) :
    m_impl(new QnDesktopResourceSearcherImpl(mainWidget))
{
}

QnDesktopResourceSearcher::~QnDesktopResourceSearcher()
{
}

QString QnDesktopResourceSearcher::manufacture() const
{
    return kManufacture;
}

QnResourceList QnDesktopResourceSearcher::findResources()
{
    return m_impl->findResources();
}

bool QnDesktopResourceSearcher::isResourceTypeSupported(QnUuid resourceTypeId) const
{
    QN_UNUSED(resourceTypeId);
    return false;
}

QnResourcePtr QnDesktopResourceSearcher::createResource(
    const QnUuid &resourceTypeId,
    const QnResourceParams &params)
{
    QN_UNUSED(resourceTypeId);
    QN_UNUSED(params);
    return QnResourcePtr();
}


