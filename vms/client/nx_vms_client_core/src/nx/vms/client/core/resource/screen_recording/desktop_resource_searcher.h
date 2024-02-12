// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_management/resource_searcher.h>
#include <nx/utils/singleton.h>

struct QnAbstractDesktopResourceSearcherImpl;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API DesktopResourceSearcher:
    public QObject,
    public QnAbstractResourceSearcher
{
    Q_OBJECT
    using base_type = QObject;

public:
    DesktopResourceSearcher(QnAbstractDesktopResourceSearcherImpl* impl, QObject* parent = nullptr);
    virtual ~DesktopResourceSearcher();

    virtual QString manufacturer() const override;

    virtual QnResourceList findResources() override;

    virtual bool isResourceTypeSupported(nx::Uuid resourceTypeId) const override;

    virtual bool isVirtualResource() const override { return true; }

protected:
    virtual QnResourcePtr createResource(const nx::Uuid& resourceTypeId,
        const QnResourceParams& params) override;

private:
    std::unique_ptr<QnAbstractDesktopResourceSearcherImpl> m_impl;
};

} // namespace nx::vms::client::core
