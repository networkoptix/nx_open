#ifndef QN_DESKTOP_RESOURCE_SEARCHER_H
#define QN_DESKTOP_RESOURCE_SEARCHER_H

#include "core/resource_managment/resource_searcher.h"

struct IDirect3D9;

class QnDesktopResourceSearcher : public QnAbstractResourceSearcher
{
    QnDesktopResourceSearcher();

public:
    ~QnDesktopResourceSearcher();

    static QnDesktopResourceSearcher& instance();

    virtual QString manufacture() const override;

    virtual QnResourceList findResources() override;

    virtual bool isResourceTypeSupported(QnId resourceTypeId) const override;

protected:
    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters) override;

private:
    IDirect3D9 *m_pD3D;
};

#endif // QN_DESKTOP_RESOURCE_SEARCHER_H
