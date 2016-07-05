#pragma once
#include <core/resource_management/resource_searcher.h>
#include <nx/utils/singleton.h>

class QGLWidget;
class QnDesktopResourceSearcherImpl;

class QnDesktopResourceSearcher :
    public QObject,
    public QnAbstractResourceSearcher
{
    Q_OBJECT

   using base_type = QObject;
public:
    QnDesktopResourceSearcher(QGLWidget* mainWidget, QObject* parent = nullptr);
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
