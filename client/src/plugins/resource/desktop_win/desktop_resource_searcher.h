#ifndef QN_DESKTOP_RESOURCE_SEARCHER_H
#define QN_DESKTOP_RESOURCE_SEARCHER_H

#ifdef _WIN32

#include "core/resource_management/resource_searcher.h"

struct IDirect3D9;

class QnDesktopResourceSearcher : public QnAbstractResourceSearcher
{
public:
    QnDesktopResourceSearcher(QGLWidget* mainWidget);
    ~QnDesktopResourceSearcher();

    static QnDesktopResourceSearcher& instance();

    virtual QString manufacture() const override;

    virtual QnResourceList findResources() override;

    virtual bool isResourceTypeSupported(QUuid resourceTypeId) const override;

    static void initStaticInstance(QnDesktopResourceSearcher* instance);

    virtual bool isVirtualResource() const override { return true; }

protected:
    virtual QnResourcePtr createResource(const QUuid &resourceTypeId, const QnResourceParams& params) override;

private:
    IDirect3D9 *m_pD3D;
    QGLWidget* m_mainWidget;
};

#endif

#endif // QN_DESKTOP_RESOURCE_SEARCHER_H
