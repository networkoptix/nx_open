// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_management/resource_searcher.h>
#include <plugins/resource/desktop_camera/abstract_desktop_resource_searcher_impl.h>

#include <nx/utils/singleton.h>

class NX_VMS_CLIENT_CORE_API QnDesktopResourceSearcher:
    public QObject,
    public QnAbstractResourceSearcher
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnDesktopResourceSearcher(QnAbstractDesktopResourceSearcherImpl* impl, QObject* parent = nullptr);
    virtual ~QnDesktopResourceSearcher();

    virtual QString manufacturer() const override;

    virtual QnResourceList findResources() override;

    virtual bool isResourceTypeSupported(QnUuid resourceTypeId) const override;

    virtual bool isVirtualResource() const override { return true; }

protected:
    virtual QnResourcePtr createResource(const QnUuid& resourceTypeId,
        const QnResourceParams& params) override;

private:
    std::unique_ptr<QnAbstractDesktopResourceSearcherImpl> m_impl;
};
