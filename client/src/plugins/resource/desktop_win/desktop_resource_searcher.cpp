#ifdef _WIN32

#include <QGLWidget>

#include "desktop_resource_searcher.h"
#include "desktop_resource.h"

#ifdef Q_OS_WIN
#   include <windows.h>
#   include <shellapi.h>
#   include <commdlg.h>
#   include <d3d9.h>
#   include <d3dx9.h>
#endif

#define ONLY_PRIMARY_DESKTOP

static QnDesktopResourceSearcher* inst = 0;

QnDesktopResourceSearcher &QnDesktopResourceSearcher::instance()
{
    
    return *inst;
}

void QnDesktopResourceSearcher::initStaticInstance(QnDesktopResourceSearcher* searcher)
{
    //Q_ASSERT(inst == 0); 
    inst = searcher;
}

QnDesktopResourceSearcher::QnDesktopResourceSearcher(QGLWidget* mainWidget)
{
    m_mainWidget = mainWidget;
#ifdef Q_OS_WIN
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
#else 
    m_pD3D = NULL;
#endif
}

QnDesktopResourceSearcher::~QnDesktopResourceSearcher()
{
#ifdef Q_OS_WIN
    if (m_pD3D)
        m_pD3D->Release();
#endif
}

QString QnDesktopResourceSearcher::manufacture() const {
    return QLatin1String("NetworkOptix");
}

QnResourceList QnDesktopResourceSearcher::findResources() {
#ifdef Q_OS_WIN
    QnResourceList result;
    if (m_pD3D == 0)
        return result;

    D3DDISPLAYMODE ddm;
    for(int i = 0;; i++) {
#ifdef ONLY_PRIMARY_DESKTOP
        if(i > 0)
            break;
#endif
        if(FAILED(m_pD3D->GetAdapterDisplayMode(i, &ddm)))
            break;

        QnResourcePtr dev(new QnDesktopResource(m_mainWidget));
        result.push_back(dev);
    }
    return result;
#else
    return QnResourceList();
#endif
}

bool QnDesktopResourceSearcher::isResourceTypeSupported(QUuid resourceTypeId) const {
    Q_UNUSED(resourceTypeId)

    return false;
}

QnResourcePtr QnDesktopResourceSearcher::createResource(const QUuid &resourceTypeId, const QnResourceParams& params) {
    Q_UNUSED(resourceTypeId)
    Q_UNUSED(params)

    return QnResourcePtr(0);
}

#endif
