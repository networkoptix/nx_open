// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_resource_searcher.h"

#include <client_core/client_core_module.h>

namespace {

const QString kManufacturer = "Desktop camera";

} // namespace

QnDesktopResourceSearcher::QnDesktopResourceSearcher(
    QnAbstractDesktopResourceSearcherImpl* impl,
    QObject* parent)
    :
    base_type(parent),
    QnAbstractResourceSearcher(qnClientCoreModule->commonModule()),
    m_impl(impl)
{
}

QnDesktopResourceSearcher::~QnDesktopResourceSearcher()
{
}

QString QnDesktopResourceSearcher::manufacturer() const
{
    return kManufacturer;
}

QnResourceList QnDesktopResourceSearcher::findResources()
{
    return m_impl->findResources();
}

bool QnDesktopResourceSearcher::isResourceTypeSupported(QnUuid /*resourceTypeId*/) const
{
    return false;
}

QnResourcePtr QnDesktopResourceSearcher::createResource(
    const QnUuid& /*resourceTypeId*/,
    const QnResourceParams& /*params*/)
{
    return QnResourcePtr();
}
