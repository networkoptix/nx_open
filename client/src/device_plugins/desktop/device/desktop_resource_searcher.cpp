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

QnDesktopResourceSearcher &QnDesktopResourceSearcher::instance()
{
    static QnDesktopResourceSearcher inst;
    return inst;
}

QnDesktopResourceSearcher::QnDesktopResourceSearcher()
{
#ifdef Q_OS_WIN
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
#else 
    m_pD3D = NULL;
#endif
}

QnDesktopResourceSearcher::~QnDesktopResourceSearcher()
{
#ifdef Q_OS_WIN
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

        QnResourcePtr dev(new QnDesktopResource(i));
        result.push_back(dev);
    }
    return result;
#else
    return QnResourceList();
#endif
}

bool QnDesktopResourceSearcher::isResourceTypeSupported(QnId resourceTypeId) const {
    Q_UNUSED(resourceTypeId)

    return false;
}

QnResourcePtr QnDesktopResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters) {
    Q_UNUSED(resourceTypeId)
    Q_UNUSED(parameters)

    return QnResourcePtr(0);
}


