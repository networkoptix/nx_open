#pragma once
#include <core/resource_management/resource_searcher.h>
#include <utils/common/singleton.h>

class QGLWidget;
class QnDesktopResourceSearcherImpl;

class QnDesktopResourceSearcher :
    public QnAbstractResourceSearcher
{

public:
    QnDesktopResourceSearcher(QGLWidget* mainWidget);
    ~QnDesktopResourceSearcher();

    virtual QString manufacture() const override;

    virtual QnResourceList findResources() override;

    virtual bool isResourceTypeSupported(QnUuid resourceTypeId) const override;

    virtual bool isVirtualResource() const override { return true; }

protected:
    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

private:

    std::unique_ptr<QnDesktopResourceSearcherImpl> m_impl;
};
